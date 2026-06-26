import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../fs/src/index.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { launchSystem } from '../orchestrator.js';
import { waitForMeshNodes } from './vfs_test_helpers.js';

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

test('Decentralized Mesh-VFS Integration via Zenoh', { timeout: 60000 }, async (t) => {
  const sys = await launchSystem('test/standard');
  const ROUTER_PORT = sys.ports.zenoh_router;
  const routerUrl = `http://localhost:${ROUTER_PORT}`;

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

  const meshA = new MeshLink(vfsA, [routerUrl]);
  const meshB = new MeshLink(vfsB, [routerUrl]);
  const meshC = new MeshLink(vfsC, [routerUrl]);

  await Promise.all([meshA.start(), meshB.start(), meshC.start()]);

  // Wait for mesh to settle: Node A needs to see Node C
  console.log('[Test Mesh] Waiting for Node C to be visible...');
  await waitForMeshNodes(vfsA, ['node-C'], 5000);

  await t.test('should implement Ephemeral Wipe on startup', async () => {
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
    const meshD = new MeshLink(vfsD, [routerUrl]);
    await meshD.start();
    
    // Node A should now see Node D in discovery
    await waitForMeshNodes(vfsA, ['node-D'], 5000);
    
    assert.ok(meshA.discoveredProviders.has('node-D'), 'Node A should have registered Node D');
    console.log('[Test Mesh] AUTO-PEERING SUCCESSful.');
    
    await meshD.stop();
    await vfsD.close();
  });

  t.after(async () => {
    console.log('[Test Mesh] Cleaning up all nodes...');
    await Promise.all([meshA.stop(), meshB.stop(), meshC.stop()]);
    await Promise.all([vfsA.close(), vfsB.close(), vfsC.close()]);
    await sys.stop();
  });
});
