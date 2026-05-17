import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
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

const PORT_OPS = 20201;
const PORT_EXPORT = 20202;
const PORT_CLIENT = 20203;

test('Full Mesh Pipeline (C++ Ops + JS Export)', { timeout: 10000 }, async (t) => {
  let opsProcess, exportProcess, clientServer, clientVfs, mesh;
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

    if (opsProcess) opsProcess.kill('SIGKILL');
    if (exportProcess) exportProcess.kill('SIGKILL');

    await fs.rm(path.resolve('.vfs_storage_pipeline-ops'), { recursive: true, force: true }).catch(() => {});
    await fs.rm(path.resolve('.vfs_storage_pipeline-export'), { recursive: true, force: true }).catch(() => {});
    await fs.rm(path.resolve('.vfs_storage_pipeline-client'), { recursive: true, force: true }).catch(() => {});
    await fs.rm(path.resolve(filename), { force: true }).catch(() => {});
    console.log('[Test Pipeline] Cleanup complete.');
  });

  // 1. Start C++ Ops Node (Leaf)
  const __dirname = path.dirname(new URL(import.meta.url).pathname);
  const root = path.resolve(__dirname, '..');
  const opsBin = path.resolve(__dirname, '../geo/bin/ops');
  opsProcess = spawn(opsBin, [PORT_OPS.toString()], {
    cwd: root,
    env: { ...process.env, PEER_ID: 'pipeline-ops' },
  });
  opsProcess.stdout.on('data', (d) => console.log(`[OPS] ${d}`));
  opsProcess.stderr.on('data', (d) => console.error(`[OPS ERR] ${d}`));

  // 2. Start JS Export Node (Peered with Ops)
  console.log('[Test Pipeline] Launching JS Export Node...');
  const exportService = path.resolve(__dirname, '../geo/export_service.js');
  exportProcess = spawn('node', [exportService], {
    cwd: root,
    env: {
      ...process.env,
      PORT: PORT_EXPORT.toString(),
      PEER_ID: 'pipeline-export',
      NEIGHBORS: `http://localhost:${PORT_OPS}`,
    },
  });
  exportProcess.stdout.on('data', (d) => console.log(`[EXPORT] ${d}`));
  exportProcess.stderr.on('data', (d) => console.error(`[EXPORT ERR] ${d}`));

  // Wait for nodes to be healthy
  const waitForHealth = async (url, name) => {
    console.log(`[Test Pipeline] Waiting for ${name} health...`);
    for (let i = 0; i < 20; i++) {
      try {
        const resp = await fetch(`${url}/health`);
        if (resp.ok) {
          console.log(`[Test Pipeline] ${name} is healthy.`);
          return;
        }
      } catch (e) {}
      await new Promise((r) => setTimeout(r, 500));
    }
    throw new Error(`${name} failed to become healthy`);
  };

  await waitForHealth(`http://localhost:${PORT_OPS}`, 'Ops Node');
  await waitForHealth(`http://localhost:${PORT_EXPORT}`, 'Export Node');

  // 3. Setup JS Client Node (Peered with Export)
  console.log('[Test Pipeline] Starting JS Test Client...');
  clientVfs = new VFS({
    id: 'pipeline-client',
    storage: new DiskStorage('.vfs_storage_pipeline-client'),
  });
  mesh = new MeshLink(clientVfs, [`http://localhost:${PORT_EXPORT}`], {
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

      assert.ok(bytes, 'Export should return data');
      
      const stats = await fs.stat(path.resolve(filename));
      assert.ok(stats.size > 100, 'PDF file should be generated and non-empty');
      console.log(`[Test Pipeline] SUCCESS: Generated ${stats.size} byte PDF.`);
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

    const resp = await fetch(`http://localhost:${PORT_OPS}/read`, {
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
