import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import { launchSystem } from '../orchestrator.js';
import { encodeRecord } from '../fs/src/mesh/forward_connection.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

test('Node.js <-> C++ Pub-Sub Integration', async (t) => {
    // 1. Launch the TEST system
    const sys = await launchSystem('test/standard');
    const EXPORT_PORT = sys.ports.export;
    const ROUTER_PORT = sys.ports.zenoh_router;

    const hasCerts = fs.existsSync(path.join(__dirname, '../.ssl/localhost-key.pem')) && 
                     fs.existsSync(path.join(__dirname, '../.ssl/localhost-cert.pem'));
    const protocol = hasCerts ? 'https' : 'http';

    // Initialize VFS & Mesh
    const nodeVFS = new VFS({ id: 'node-js' });
    const mesh = new MeshLink(nodeVFS, [`http://localhost:${ROUTER_PORT}`]);
    await nodeVFS.init();

    try {
        // 3. Connect Node.js -> C++
        console.log('[Test] Connecting Node.js to Zenoh router...');
        await mesh.start();
        
        // Wait for Ops Node to see us
        const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
        await waitForMeshNodes(nodeVFS, ['geo-ops-node']);
        
        // 4. Test 1: Interest Propagation (Node -> C++)
        console.log('[Test] Subscribing from Node.js to test/topic...');
        const topic = new Selector('test/topic');
        let receivedByNode = null;
        
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'test/topic') {
                receivedByNode = payload;
            }
        });

        await nodeVFS.subscribe(topic);
        console.log('✔ Subscription interest sent');

        // 5. Test 2: Notification Routing (C++ -> Node)
        console.log('[Test] Triggering notification from Export Node...');
        const body = encodeRecord({
            op: 'PUB',
            selector: topic.toJSON()
        }, new TextEncoder().encode(JSON.stringify({ hello: 'from-cpp' })));

        const notifyRes = await fetch(`${protocol}://localhost:${EXPORT_PORT}/notify`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/octet-stream'
            },
            body: body
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
            catalogReceived.provider === 'geo-ops-node' || 
            catalogReceived.provider.includes('standard_ops') || 
            catalogReceived.provider.includes('standard_export'),
            `Expected geo-ops-node or standard peer ID, got: ${catalogReceived.provider}`
        );
        console.log('✔ Node.js received Catalog from C++');

    } finally {
        mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
