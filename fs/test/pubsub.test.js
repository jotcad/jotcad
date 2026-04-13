import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';
import { MeshLink } from '../src/mesh_link.js';

test('Mesh Pub-Sub: Propagation & Backflow Prevention', async (t) => {
    // Setup a 3-node chain: A <-> B <-> C
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

    // Mock direct peering (bypass HTTP for unit test)
    const connect = (mA, mB) => {
        const peerA = {
            id: mB.vfs.id,
            subscribe: (t, e, s) => { mB.addInterest(mA.vfs.id, t, e, s); return Promise.resolve(); },
            notify: (t, p, s) => { mB.notify(t, p, s); return Promise.resolve(); }
        };
        const peerB = {
            id: mA.vfs.id,
            subscribe: (t, e, s) => { mA.addInterest(mB.vfs.id, t, e, s); return Promise.resolve(); },
            notify: (t, p, s) => { mA.notify(t, p, s); return Promise.resolve(); }
        };
        mA.peers.set(mB.vfs.id, peerA);
        mB.peers.set(mA.vfs.id, peerB);
    };

    connect(meshA, meshB);
    connect(meshB, meshC);

    await t.test('subscription should propagate through the chain', async () => {
        const topic = 'test/topic';
        const expiry = Date.now() + 10000;

        // A subscribes
        await meshA.subscribe(topic, expiry);

        // Verify B knows A is interested
        const bInterests = meshB.interests.get(topic);
        assert.ok(bInterests && bInterests.has('node-A'), 'Node B should record Node A interest');

        // Verify C knows B is interested (propagation)
        const cInterests = meshC.interests.get(topic);
        assert.ok(cInterests && cInterests.has('node-B'), 'Node C should record Node B interest');
    });

    await t.test('notifications should route back to subscribers', async () => {
        const topic = 'test/topic';
        let receivedByA = null;

        // A listens for pulses
        const originalNotifyA = meshA.notify.bind(meshA);
        meshA.notify = (t, p, s) => {
            if (t === topic) receivedByA = { payload: p, stack: s };
            originalNotifyA(t, p, s);
        };

        // C publishes
        const payload = { status: 'WORKING' };
        meshC.notify(topic, payload);

        // Wait for microtasks
        await new Promise(r => setTimeout(r, 20));

        assert.ok(receivedByA !== null, 'Node A should receive notification from C');
        assert.deepStrictEqual(receivedByA.payload, payload);
        
        // Path was C -> B -> A
        assert.ok(receivedByA.stack.includes('node-C'), 'Stack should include origin node C');
        assert.ok(receivedByA.stack.includes('node-B'), 'Stack should include intermediate node B');
    });

    await t.test('backflow prevention should stop infinite loops', async () => {
        // Create a cycle: C -> A
        connect(meshC, meshA);

        // B must have an interest to forward it
        meshB.addInterest('node-C', 'loop/topic', Date.now() + 10000);

        let publishCount = 0;
        const originalNotifyB = meshB.notify.bind(meshB);
        meshB.notify = (t, p, s) => {
            if (t === 'loop/topic') publishCount++;
            return originalNotifyB(t, p, s);
        };

        // Publish from A
        meshA.notify('loop/topic', { data: 1 });

        await new Promise(r => setTimeout(r, 100));

        // In a loop A -> B -> C -> A -> B...
        // With stack protection, B should only be notified ONCE
        assert.strictEqual(publishCount, 1, 'Notification should only pass through Node B once');
    });
});
