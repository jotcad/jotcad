import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { existsSync } from 'node:fs';

const TEST_STORAGE_A = path.join(process.cwd(), '.test_vfs_a');
const TEST_STORAGE_B = path.join(process.cwd(), '.test_vfs_b');
const TEST_STORAGE_C = path.join(process.cwd(), '.test_vfs_c');
const TEST_STORAGE_D = path.join(process.cwd(), '.test_vfs_d');

async function createNode(id, port, neighbors = [], storageDir) {
  const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
  const localUrl = `http://localhost:${port}/vfs`;
  const mesh = new MeshLink(vfs, neighbors, { localUrl });
  const server = http.createServer();
  const stopServer = registerVFSRoutes(vfs, server, '/vfs', mesh);
  await new Promise((resolve) => server.listen(port, '0.0.0.0', resolve));
  await vfs.init();
  await mesh.start();
  return { vfs, mesh, server, port, id, storageDir, localUrl, stopServer };
}

test('Decentralized Mesh-VFS Integration', async (t) => {
  let nodeA, nodeB, nodeC;

  t.after(async () => {
    console.log('[Test Mesh] Cleaning up all nodes...');
    if (nodeA) {
      nodeA.stopServer();
      nodeA.server.close();
      await nodeA.vfs.close();
    }
    if (nodeB) {
      nodeB.stopServer();
      nodeB.server.close();
      await nodeB.vfs.close();
    }
    if (nodeC) {
      nodeC.stopServer();
      nodeC.server.close();
      await nodeC.vfs.close();
    }

    await fs
      .rm(TEST_STORAGE_A, { recursive: true, force: true })
      .catch(() => {});
    await fs
      .rm(TEST_STORAGE_B, { recursive: true, force: true })
      .catch(() => {});
    await fs
      .rm(TEST_STORAGE_C, { recursive: true, force: true })
      .catch(() => {});
    await fs
      .rm(TEST_STORAGE_D, { recursive: true, force: true })
      .catch(() => {});
  });

  // Initialize
  nodeC = await createNode('node-c', 9102, [], TEST_STORAGE_C);
  nodeB = await createNode('node-b', 9101, [nodeC.localUrl], TEST_STORAGE_B);
  nodeA = await createNode('node-a', 9100, [nodeB.localUrl], TEST_STORAGE_A);

  await t.test('should implement Ephemeral Wipe on startup', async () => {
    const dummyFile = path.join(TEST_STORAGE_C, 'ghost.meta');
    await fs.writeFile(dummyFile, 'phantom data');
    assert.ok(existsSync(dummyFile));

    await nodeC.vfs.close();
    nodeC.server.close();

    nodeC = await createNode('node-c', 9102, [], TEST_STORAGE_C);
    assert.ok(!existsSync(dummyFile));
  });

  await t.test(
    'should fulfill a recursive Bread-crumb READ (A -> B -> C)',
    async () => {
      const vfsPath = 'mesh/integration/test';
      const vfsParams = { version: '1.0.0' };
      const vfsContent = { message: 'Hello from the far end!' };
      await nodeC.vfs.writeData(vfsPath, vfsParams, vfsContent);

      console.log('[Test Mesh] Triggering recursive READ from Node A...');
      const result = await nodeA.vfs.readData(vfsPath, vfsParams);
      assert.deepStrictEqual(result, vfsContent);
      console.log('[Test Mesh] READ SUCCESSful.');
    }
  );

  await t.test('should implement Auto-Peering (Symmetry)', async () => {
    const nodeD = await createNode(
      'node-d',
      9103,
      [nodeC.localUrl],
      TEST_STORAGE_D
    );
    assert.ok(!nodeC.mesh.peers.has(nodeD.id));

    const p = 'mesh/peering/test';
    await nodeC.vfs.writeData(p, {}, 'peering check');
    await nodeD.vfs.readData(p, {});

    assert.ok(nodeC.mesh.peers.has(nodeD.id));
    console.log('[Test Mesh] AUTO-PEERING SUCCESSful.');

    await nodeD.vfs.close();
    nodeD.server.close();
  });
});
