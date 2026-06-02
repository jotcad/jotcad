import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';
import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
  getSelectorKey,
  Selector,
} from '../fs/src/index.js';

import { launchSystem, PROFILES } from '../orchestrator.js';

async function consumeBytes(stream) {
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

async function consumeJSON(stream) {
    const bytes = await consumeBytes(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('C++ Native Node Integration', { timeout: 60000 }, async (t) => {
  let sys;
  let server;
  let jsVfs;
  let stopServer;

  const PORT_JS = 20102;
  const STORAGE_JS = path.resolve('.test_vfs_cpp_integration_js');

  t.after(async () => {
    console.log('[Test] Cleaning up...');
    if (stopServer) stopServer();
    if (server) server.close();
    if (jsVfs) await jsVfs.close();
    if (sys) await sys.stop();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
  });

  // 1. Launch the TEST system
  const profileKey = 'test/standard';
  sys = await launchSystem(profileKey);
  const PORT_CPP = sys.ports.ops;
  const STORAGE_CPP = `${PROFILES[profileKey].storagePrefix}ops`;

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-test-client',
    storage: new DiskStorage(STORAGE_JS),
  });
  const mesh = new MeshLink(jsVfs, [`http://localhost:${PORT_CPP}`], {
    localUrl: `http://localhost:${PORT_JS}`,
  });
  server = http.createServer();
  stopServer = registerVFSRoutes(jsVfs, server, '', mesh);

  await new Promise((resolve) => server.listen(PORT_JS, '0.0.0.0', resolve));
  await jsVfs.init();
  await mesh.start();

  await t.test('Health Check', async () => {
    const resp = await fetch(`http://localhost:${PORT_CPP}/health`);
    const info = await resp.json();
    assert.strictEqual(info.status, 'OK');
  });

  await t.test('Peer Registration (Exercises probeDirectReachability)', async () => {
    const resp = await fetch(`http://localhost:${PORT_JS}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: 'geo-ops-node', url: `http://localhost:${PORT_CPP}` })
    });
    assert.strictEqual(resp.status, 200, 'JS node should accept registration from C++ node');
    const info = await resp.json();
    assert.strictEqual(info.id, 'js-test-client');
  });

  await t.test('CID Consistency', async () => {
    // Note: box op with these params produces a deterministic CID
    const selector = new Selector('jot/Box', { depth: 0, height: 10, width: 10 }).withOutput('$out');
    const jsAddrKey = await getSelectorKey(selector);
    await jsVfs.read(selector);

    // In the new architecture:
    // The selector hash identifies BOTH the .meta and .data files
    const metaFile = path.join(STORAGE_CPP, `${jsAddrKey}.meta`);
    const dataFile = path.join(STORAGE_CPP, `${jsAddrKey}.data`);
    
    await fs.access(metaFile); 
    await fs.access(dataFile);
    
    const meta = JSON.parse(await fs.readFile(metaFile, 'utf8'));
    assert.strictEqual(meta.state, 'AVAILABLE');
  });

  await t.test('Provisioning: Box', async () => {
    const { stream: sStream } = await jsVfs.read(new Selector('jot/Box', {
      width: 20,
      height: 20,
      depth: 0,
    }).withOutput('$out'));
    const result = await consumeJSON(sStream);
    console.log('[Test] Box provisioning result:', JSON.stringify(result));
    assert.ok(result, 'Box provisioning should return a Shape');
    assert.strictEqual(typeof result, 'object', 'Result should be an object (Shape)');
    assert.strictEqual(result.tags?.type, 'surface');
    
    const { stream: gStream } = await jsVfs.read(result.geometry);
    const geo = await consumeBytes(gStream);
    assert.ok(geo, 'Should be able to read geometry by CID');
    const geoText = new TextDecoder().decode(geo);
    // New format is: V <count>\n<x> <y> <z>...
    // 2D Box (depth=0) has 4 vertices.
    assert.ok(geoText.includes('V 4'), 'Should contain vertex count header');
    assert.ok(geoText.includes('10'), 'Should contain x coordinate (10)');
  });

  await t.test('Provisioning: Triangle', async () => {
    const { stream } = await jsVfs.read(new Selector('jot/Triangle/equilateral', {
      size: 50,
    }).withOutput('$out'));
    const result = await consumeJSON(stream);
    console.log('[Test] Triangle provisioning result:', JSON.stringify(result));
    assert.ok(result, 'Triangle provisioning should return a Shape');
    assert.strictEqual(typeof result, 'object', 'Result should be an object (Shape)');
    assert.strictEqual(result.tags?.type, 'surface');
  });
});
