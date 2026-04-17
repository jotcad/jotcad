import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';

const STORAGE_A = path.join(process.cwd(), '.test_handshake_a');
const STORAGE_B = path.join(process.cwd(), '.test_handshake_b');

async function createNode(id, port, neighbors = [], localUrl) {
  const storageDir = id === 'a' ? STORAGE_A : STORAGE_B;
  await fs.rm(storageDir, { recursive: true, force: true }).catch(() => {});
  const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
  const url = localUrl || `http://127.0.0.1:${port}/vfs`;
  const mesh = new MeshLink(vfs, neighbors, { localUrl: url });
  const server = http.createServer();
  const stopServer = registerVFSRoutes(vfs, server, '/vfs', mesh);
  await new Promise((resolve) => server.listen(port, '127.0.0.1', resolve));
  await vfs.init();
  return { vfs, mesh, server, port, id, localUrl: url, stopServer };
}

async function stopNode(node) {
    if (!node) return;
    node.mesh.stop();
    node.stopServer();
    node.server.close();
    await node.vfs.close();
}

test(
  'Mesh Handshake & Reachability Negotiation',
  { timeout: 10000 },
  async (t) => {
    t.after(async () => {
      await fs.rm(STORAGE_A, { recursive: true, force: true }).catch(() => {});
      await fs.rm(STORAGE_B, { recursive: true, force: true }).catch(() => {});
    });

    await t.test('should negotiate DIRECT linkage on localhost', async () => {
      const nodeB = await createNode('b', 9211);
      const nodeA = await createNode('a', 9210);

      try {
          console.log('[Test Handshake] Starting handshake from A to B...');
          const remoteId = await nodeA.mesh.addPeer(nodeB.localUrl);

          assert.strictEqual(remoteId, 'b');

          // Wait for symmetric registration to complete (it is backgrounded)
          for (let i = 0; i < 20; i++) {
            if (nodeB.mesh.peers.has('a')) break;
            await new Promise((r) => setTimeout(r, 50));
          }

          assert.ok(
            nodeB.mesh.peers.has('a'),
            'Node B should have registered Node A symmetrically'
          );
          
          const peerA = nodeB.mesh.peers.get('a');
          assert.strictEqual(peerA.reachability, 'DIRECT', 'Peer A should be DIRECT');
      } finally {
          await stopNode(nodeA);
          await stopNode(nodeB);
      }
    });

    await t.test(
      'should negotiate REVERSE fallback when direct probe fails',
      async () => {
        const nodeB = await createNode('b', 9221);
        const nodeA = await createNode('a', 9220, [], 'http://127.0.0.1:9999/broken');

        try {
            console.log('[Test Handshake] Starting handshake with bogus URL...');
            await nodeA.mesh.addPeer(nodeB.localUrl);

            for (let i = 0; i < 20; i++) {
              const peerA = nodeB.mesh.peers.get('a');
              if (peerA && peerA.reachability === 'REVERSE') break;
              await new Promise((r) => setTimeout(r, 50));
            }

            const peerA = nodeB.mesh.peers.get('a');
            assert.ok(peerA, 'Node B should have peer A');
            assert.strictEqual(peerA.reachability, 'REVERSE', 'Unreachable peer should be REVERSE');
            console.log('[Test Handshake] REVERSE fallback SUCCESSful.');
        } finally {
            await stopNode(nodeA);
            await stopNode(nodeB);
        }
      }
    );

    await t.test('should be idempotent (multiple addPeer calls)', async () => {
      const nodeB = await createNode('b', 9231);
      const nodeA = await createNode('a', 9230);

      try {
          let registerCalls = 0;
          const originalFetch = nodeA.mesh.fetch;
          nodeA.mesh.fetch = async (url, options) => {
            if (url.endsWith('/register')) registerCalls++;
            return originalFetch(url, options);
          };

          console.log('[Test Handshake] Triggering 10 parallel addPeer calls...');
          await Promise.all(
            Array(10)
              .fill(0)
              .map(() => nodeA.mesh.addPeer(nodeB.localUrl))
          );

          assert.strictEqual(
            registerCalls,
            1,
            'Should only call /register ONCE for the same URL'
          );
          console.log('[Test Handshake] Idempotency SUCCESSful.');
      } finally {
          await stopNode(nodeA);
          await stopNode(nodeB);
      }
    });
  }
);
