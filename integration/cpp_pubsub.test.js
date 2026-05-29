import test from 'node:test';
import assert from 'node:assert';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import { launchSystem, PROFILES } from '../orchestrator.js';

test('Node.js <-> C++ Pub-Sub Integration', async (t) => {
    // 1. Launch the TEST system
    const sys = await launchSystem(PROFILES.TEST);
    const CPP_PORT = sys.ports.ops;

    // Initialize VFS & Mesh
    const nodeVFS = new VFS({ id: 'node-js' });
    const mesh = new MeshLink(nodeVFS);
    await nodeVFS.init();

    try {
        // 3. Connect Node.js -> C++
        console.log('[Test] Connecting Node.js to C++...');
        await mesh.addPeer(`http://localhost:${CPP_PORT}`);
        
        // 4. Test 1: Interest Propagation (Node -> C++)
        console.log('[Test] Subscribing from Node.js to test/topic...');
        const topic = { path: 'test/topic' };
        let receivedByNode = null;
        
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'test/topic') {
                receivedByNode = payload;
            }
        });

        // We need to wait a bit for the C++ node to process the subscription
        await mesh.subscribe(Selector.fromObject(topic), Date.now() + 10000);
        console.log('✔ Subscription interest sent');

        // 5. Test 2: Notification Routing (C++ -> Node)
        console.log('[Test] Triggering notification from C++...');
        const notifyRes = await fetch(`http://localhost:${CPP_PORT}/notify`, {
            method: 'POST',
            body: JSON.stringify({
                selector: topic,
                payload: { hello: 'from-cpp' },
                stack: []
            })
        });
        assert.strictEqual(notifyRes.status, 200);

        // Wait for Node.js to receive it (up to 5s)
        let attempts = 0;
        while (!receivedByNode && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(receivedByNode, 'Node.js should have received notification');
        assert.strictEqual(receivedByNode.hello, 'from-cpp');
        console.log('✔ Node.js received notification from C++');

        // 6. Test 3: Catalog Reply (sys/schema)
        console.log('[Test] Testing sys/schema catalog reply...');
        let catalogReceived = null;
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'sys/schema') {
                catalogReceived = payload;
            }
        });

        await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);
        
        attempts = 0;
        while (!catalogReceived && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(
            catalogReceived.provider === 'live_standard_ops' || 
            catalogReceived.provider === 'test_standard_ops' || 
            catalogReceived.provider === 'geo-ops-node' ||
            catalogReceived.provider === 'live_standard_export' ||
            catalogReceived.provider === 'test_standard_export',
            `Expected dynamic ops/export peer ID, got: ${catalogReceived.provider}`
        );
        console.log('✔ Node.js received Catalog from C++');

    } finally {
        mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
