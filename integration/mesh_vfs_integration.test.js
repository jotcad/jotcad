import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, MemoryStorage, Selector } from '../fs/src/index.js';

async function consumeJSON(stream) {
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
    return JSON.parse(new TextDecoder().decode(bytes));
}

import { waitForMeshNodes } from './vfs_test_helpers.js';

test('Decentralized Mesh-VFS Integration', { timeout: 30000 }, async (t) => {
  const vfsA = new VFS({ id: 'node-A', storage: new MemoryStorage() });
  const vfsB = new VFS({ id: 'node-B', storage: new MemoryStorage() });
  const vfsC = new VFS({ id: 'node-C', storage: new MemoryStorage() });

  await vfsA.init();
  await vfsB.init();
  await vfsC.init();

  // Provision terminal data on Node C
  const target = new Selector('far-end/data', { secret: 'gold' });
  const payload = { message: 'Hello from the far end!' };
  await vfsC.write(target, payload, { encoding: 'json' });

  const meshA = new MeshLink(vfsA, ['http://localhost:25002'], { localUrl: 'http://localhost:25001' });
  const meshB = new MeshLink(vfsB, ['http://localhost:25001', 'http://localhost:25003'], { localUrl: 'http://localhost:25002' });
  const meshC = new MeshLink(vfsC, ['http://localhost:25002'], { localUrl: 'http://localhost:25003' });

  const serverA = http.createServer(); registerVFSRoutes(vfsA, serverA, '', meshA); serverA.listen(25001);
  const serverB = http.createServer(); registerVFSRoutes(vfsB, serverB, '', meshB); serverB.listen(25002);
  const serverC = http.createServer(); registerVFSRoutes(vfsC, serverC, '', meshC); serverC.listen(25003);

  serverA.unref(); serverB.unref(); serverC.unref();

  await Promise.all([meshA.start(), meshB.start(), meshC.start()]);

  // Wait for mesh to settle: Node A needs to discover Node B and Node C
  console.log('[Test Mesh] Waiting for A -> B -> C routing path to form...');
  await waitForMeshNodes(vfsA, ['node-B', 'node-C']);


  await t.test('should implement Ephemeral Wipe on startup', async () => {
    // MemoryStorage is empty by default on startup
    const files = await vfsA.storage.iterateMeta();
    let count = 0; for await (const f of files) count++;
    assert.strictEqual(count, 0);
  });

  await t.test('should fulfill a recursive Bread-crumb READ (A -> B -> C)', async () => {
    console.log('[Test Mesh] Triggering recursive READ from Node A...');
    const result = await vfsA.read(target);
    assert.ok(result, 'Result should exist');
    const data = await consumeJSON(result.stream);
    assert.deepStrictEqual(data, payload);
  });

  await t.test('should implement Auto-Peering (Symmetry)', async () => {
    const vfsD = new VFS({ id: 'node-D', storage: new MemoryStorage() });
    const meshD = new MeshLink(vfsD, ['http://localhost:25001']);
    await meshD.start();
    
    // Node A should now see Node D as a peer even though D only contacted A
    await waitForMeshNodes(vfsA, ['node-D']);
    
    assert.ok(meshA.peers.has('node-D'), 'Node A should have auto-registered Node D');
    console.log('[Test Mesh] AUTO-PEERING SUCCESSful.');
    
    meshD.stop();
    await vfsD.close();
  });

  t.after(async () => {
    console.log('[Test Mesh] Cleaning up all nodes...');
    
    // Force close all connections to prevent hanging
    for (const s of [serverA, serverB, serverC]) {
        if (s.closeAllConnections) s.closeAllConnections();
        s.close();
    }
    
    meshA.stop(); meshB.stop(); meshC.stop();
    await vfsA.close(); await vfsB.close(); await vfsC.close();
  });
});
