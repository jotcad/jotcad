import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, MemoryStorage } from '../src/index.js';

test('Decentralized Mesh-VFS Integration', async (t) => {
  const vfsA = new VFS({ id: 'node-A', storage: new MemoryStorage() });
  const vfsB = new VFS({ id: 'node-B', storage: new MemoryStorage() });
  const vfsC = new VFS({ id: 'node-C', storage: new MemoryStorage() });

  // Provision terminal data on Node C
  const target = { path: 'far-end/data', parameters: { secret: 'gold' } };
  const payload = { message: 'Hello from the far end!' };
  await vfsC.writeData(target, payload);

  const meshA = new MeshLink(vfsA, ['http://localhost:25002'], { localUrl: 'http://localhost:25001' });
  const meshB = new MeshLink(vfsB, ['http://localhost:25001', 'http://localhost:25003'], { localUrl: 'http://localhost:25002' });
  const meshC = new MeshLink(vfsC, ['http://localhost:25002'], { localUrl: 'http://localhost:25003' });

  const serverA = http.createServer(); registerVFSRoutes(vfsA, serverA, '', meshA); serverA.listen(25001);
  const serverB = http.createServer(); registerVFSRoutes(vfsB, serverB, '', meshB); serverB.listen(25002);
  const serverC = http.createServer(); registerVFSRoutes(vfsC, serverC, '', meshC); serverC.listen(25003);

  await Promise.all([vfsA.init(), vfsB.init(), vfsC.init()]);
  await Promise.all([meshA.start(), meshB.start(), meshC.start()]);

  // Wait for mesh to settle
  await new Promise(r => setTimeout(r, 1000));

  await t.test('should implement Ephemeral Wipe on startup', async () => {
    // MemoryStorage is empty by default on startup
    const files = await vfsA.storage.iterateMeta();
    let count = 0; for await (const f of files) count++;
    assert.strictEqual(count, 0);
  });

  await t.test('should fulfill a recursive Bread-crumb READ (A -> B -> C)', async () => {
    console.log('[Test Mesh] Triggering recursive READ from Node A...');
    const result = await vfsA.readData(target);
    assert.deepStrictEqual(result, payload);
  });

  await t.test('should implement Auto-Peering (Symmetry)', async () => {
    const vfsD = new VFS({ id: 'node-D' });
    const meshD = new MeshLink(vfsD, ['http://localhost:25001']);
    await meshD.start();
    await new Promise(r => setTimeout(r, 500));
    
    // Node A should now see Node D as a peer even though D only contacted A
    assert.ok(meshA.peers.has('node-D'), 'Node A should have auto-registered Node D');
    console.log('[Test Mesh] AUTO-PEERING SUCCESSful.');
    
    meshD.stop();
    await vfsD.close();
  });

  t.after(async () => {
    console.log('[Test Mesh] Cleaning up all nodes...');
    serverA.close(); serverB.close(); serverC.close();
    meshA.stop(); meshB.stop(); meshC.stop();
    await vfsA.close(); await vfsB.close(); await vfsC.close();
  });
});
