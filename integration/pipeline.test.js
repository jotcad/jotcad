import test from 'node:test';
import assert from 'node:assert';
import {
  VFS,
  MeshLink,
  Selector,
  registerVFSRoutes,
  DiskStorage,
} from '../fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import { launchSystem, PROFILES } from '../orchestrator.js';

async function consumeBinary(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return bytes;
}

test('Full Mesh Pipeline (C++ Ops + JS Export)', { timeout: 30000 }, async (t) => {
  let sys, clientServer, clientVfs, mesh;
  const filename = 'pipeline_test_result.pdf';

  t.after(async () => {
    console.log('[Test Pipeline] Cleaning up...');
    if (mesh) mesh.stop();
    if (clientVfs) await clientVfs.close();
    if (clientServer) {
      clientServer.close();
      if (typeof clientServer.closeAllConnections === 'function') {
        clientServer.closeAllConnections();
      }
    }
    if (sys) await sys.stop();

    await fs.rm(path.resolve(filename), { force: true }).catch(() => {});
    console.log('[Test Pipeline] Cleanup complete.');
  });

  // 1. Launch the TEST system (ops on 9191, export on 9192)
  sys = await launchSystem('test/standard');
  const PORT_OPS = sys.ports.ops;
  const PORT_EXPORT = sys.ports.export;
  const PORT_CLIENT = 20203;

  // Detect protocol (matches geo/export_service.js logic)
  const hasCerts = (await fs.stat('.ssl/localhost-key.pem').catch(() => null)) && 
                   (await fs.stat('.ssl/localhost-cert.pem').catch(() => null));
  const protocol = hasCerts ? 'https' : 'http';

  if (hasCerts) {
      // Allow self-signed certs for testing
      process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
  }

  // 2. Setup JS Client Node (Peered with Export)
  console.log('[Test Pipeline] Starting JS Test Client...');
  clientVfs = new VFS({
    id: 'pipeline-client',
    storage: new DiskStorage('.vfs_storage_pipeline-client'),
  });
  mesh = new MeshLink(clientVfs, [`${protocol}://localhost:${PORT_EXPORT}`], {
    localUrl: `http://localhost:${PORT_CLIENT}`,
  });
  clientServer = http.createServer();
  registerVFSRoutes(clientVfs, clientServer, '', mesh);
  await new Promise((resolve) =>
    clientServer.listen(PORT_CLIENT, '0.0.0.0', resolve)
  );
  await clientVfs.init();
  await mesh.start();

  await t.test(
    'should execute full structured pipeline across multiple language nodes',
    async () => {
      const hex = new Selector('jot/Hexagon/diameter', { diameter: 50 }).withOutput('$out');
      const offset = new Selector('jot/offset', { diameter: 5.0, $in: hex }).withOutput('$out');
      const outline = new Selector('jot/outline', { $in: offset }).withOutput('$out');
      const pipeline = new Selector('jot/pdf', { path: filename, $in: outline }).withOutput('$out');

      console.log('[Test Pipeline] Requesting structured export...');
      const res = await clientVfs.read(pipeline);
      assert.ok(res, 'Read should return a result');
      const bytes = await consumeBinary(res.stream);

      assert.ok(bytes && bytes.length > 100, 'Export should return valid PDF data');
      const header = new TextDecoder().decode(bytes.slice(0, 5));
      assert.strictEqual(header, '%PDF-', 'Result should be a valid PDF buffer');
      console.log(`[Test Pipeline] SUCCESS: Received ${bytes.length} byte PDF.`);
    }
  );

  await t.test('should reject underconstrained request to C++ op', async () => {
    clientVfs.addSchema('jot/Hexagon/diameter', {
      arguments: [
        { name: 'diameter', type: 'jot:number' }
      ]
    });

    console.log(
      '[Test Pipeline] Requesting underconstrained hexagon (no diameter)...'
    );
    try {
      // Use a Selector with empty parameters to trigger 'Missing required parameter'
      await clientVfs.read(new Selector('jot/Hexagon/diameter', {}), {});
      assert.fail('Should have been rejected locally');
    } catch (err) {
      assert.ok(err.message.includes("Missing required parameter"), `Expected validation error, got: ${err.message}`);
      console.log('[Test Pipeline] Local rejection SUCCESS:', err.message);
    }
  });

  await t.test('should reject stale request at C++ node', async () => {
    const past = Date.now() - 5000;
    console.log('[Test Pipeline] Sending raw stale request to C++ node...');

    const resp = await fetch(`http://localhost:${PORT_OPS}/read_selector`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        selector: {
            path: 'jot/Hexagon/diameter',
            parameters: { diameter: 10 },
        },
        expiresAt: past,
      }),
    });

    // Our new C++ implementation returns 404 for missing/expired/failed.
    assert.strictEqual(
      resp.status,
      404,
      'C++ node should return 404 for expired request'
    );
    console.log('[Test Pipeline] Remote C++ TTL rejection SUCCESS');
  });
});
