import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Selector } from '../fs/src/cid.js';
import { encodeRecord } from '../fs/src/mesh/forward_connection.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

runIntegrationTest('Node.js <-> C++ Pub-Sub Integration', async ({ vfs, mesh, sys }) => {
    const EXPORT_PORT = sys.ports.export;

    const hasCerts = fs.existsSync(path.join(__dirname, '../.ssl/localhost-key.pem')) && 
                     fs.existsSync(path.join(__dirname, '../.ssl/localhost-cert.pem'));
    const protocol = hasCerts ? 'https' : 'http';

    // 4. Test 1: Interest Propagation (Node -> C++)
    console.log('[Test] Subscribing from Node.js to test/topic...');
    const topic = new Selector('test/topic');
    let receivedByNode = null;
    
    vfs.events.on('notify', (selector, payload) => {
        if (selector.path === 'test/topic') {
            receivedByNode = payload;
        }
    });

    await vfs.subscribe(topic);
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
    vfs.events.on('notify', (selector, payload) => {
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
        catalogReceived.provider.startsWith('geo-') || 
        catalogReceived.provider.includes('standard_ops') || 
        catalogReceived.provider.includes('standard_export'),
        `Expected geo- node or standard peer ID, got: ${catalogReceived.provider}`
    );
    console.log('✔ Node.js received Catalog from C++');
});
