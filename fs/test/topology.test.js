import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';
import { MeshLink } from '../src/mesh_link.js';

test('Mesh Topology Discovery', async (t) => {
  // Chain: A <-> B <-> C
  const vfsA = new VFS({ id: 'node-A', storage: new MemoryStorage() });
  const vfsB = new VFS({ id: 'node-B', storage: new MemoryStorage() });
  const vfsC = new VFS({ id: 'node-C', storage: new MemoryStorage() });

  const meshA = new MeshLink(vfsA);
  const meshB = new MeshLink(vfsB);
  const meshC = new MeshLink(vfsC);

  t.after(() => {
    meshA.stop();
    meshB.stop();
    meshC.stop();
    vfsA.close();
    vfsB.close();
    vfsC.close();
  });

  // Peering
  const connect = (mA, mB) => {
    // Peer A represents Node B as seen by Node A
    const peerA = {
      id: mB.vfs.id,
      reachability: 'DIRECT',
      subscribe: (s, e, st) => {
        mB.addInterest(mA.vfs.id, s, e, st);
        return Promise.resolve();
      },
      notify: (s, p, st) => {
        return Promise.resolve(mA.notify(s, p, st));
      },
    };
    const peerB = {
      id: mA.vfs.id,
      reachability: 'DIRECT',
      subscribe: (s, e, st) => {
        mA.addInterest(mB.vfs.id, s, e, st);
        return Promise.resolve();
      },
      notify: (s, p, st) => {
        return Promise.resolve(mB.notify(s, p, st));
      },
    };
    mA.peers.set(mB.vfs.id, peerA);
    mB.peers.set(mA.vfs.id, peerB);
  };

  connect(meshA, meshB);
  connect(meshB, meshC);

  await t.test(
    'Node A should receive topology from Node C via Node B',
    async () => {
      const receivedTopo = new Map();

      // Wrap notify to log all pulses reaching A
      const originalNotifyA = meshA.notify.bind(meshA);
      meshA.notify = (selector, payload, stack) => {
        console.log(
          `[Test] Node A received pulse: ${selector.path} from ${payload.peer}`
        );
        if (payload.type === 'TOPOLOGY_UPDATE') {
          receivedTopo.set(payload.peer, payload.neighbors);
        }
        return originalNotifyA(selector, payload, stack);
      };

      // 1. Node A expresses interest in the mesh structure
      const topoSelector = { path: 'sys/topo', parameters: {} };
      console.log('[Test] Node A subscribing to sys/topo...');
      await meshA.subscribe(topoSelector, Date.now() + 10000);

      // Wait for async propagation A -> B -> C
      await new Promise((r) => setTimeout(r, 100));

      console.log('[Test] Node B Interests:', [...meshB.interests.keys()]);
      console.log('[Test] Node C Interests:', [...meshC.interests.keys()]);

      // 2. Manually trigger heartbeats
      const triggerHeartbeat = (m, name) => {
        const neighbors = [...m.peers.values()].map((p) => ({
          id: p.id,
          reachability: p.reachability,
        }));
        const sel = { path: 'sys/topo', parameters: { id: m.vfs.id } };
        console.log(
          `[Test] Triggering heartbeat from ${name} (${m.vfs.id})...`
        );
        m.notify(sel, { type: 'TOPOLOGY_UPDATE', peer: m.vfs.id, neighbors });
      };

      triggerHeartbeat(meshC, 'Node C');
      triggerHeartbeat(meshB, 'Node B');

      await new Promise((r) => setTimeout(r, 200));

      assert.ok(
        receivedTopo.has('node-C'),
        'Node A should have received Node C topology'
      );
      assert.ok(
        receivedTopo.has('node-B'),
        'Node A should have received Node B topology'
      );
    }
  );
});
