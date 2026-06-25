import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { encodeRecord } from '../fs/src/mesh/forward_connection.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

test('C++ -> Node.js Binary Notification Forwarding', async (t) => {
    // 1. Launch the TEST system (C++ Ops Node)
    const sys = await launchSystem('test/standard');
    const EXPORT_PORT = sys.ports.export;
    const ROUTER_PORT = sys.ports.zenoh_router;

    const hasCerts = fs.existsSync(path.join(__dirname, '../.ssl/localhost-key.pem')) && 
                     fs.existsSync(path.join(__dirname, '../.ssl/localhost-cert.pem'));
    const protocol = hasCerts ? 'https' : 'http';

    // 2. Initialize JS Node (Subscriber)
    const nodeVFS = new VFS({ id: 'node-js-subscriber' });
    const mesh = new MeshLink(nodeVFS, [`http://localhost:${ROUTER_PORT}`]);
    await nodeVFS.init();

    try {
        console.log('[Test] Connecting Node.js Subscriber to Zenoh Router...');
        await mesh.start();
        
        // Wait for connection to stabilize
        await new Promise(r => setTimeout(r, 1000));
        
        // 3. Subscribe JS Node to a binary topic
        const topic = new Selector('test/binary-topic');
        let receivedBinary = null;
        let receivedSelector = null;
        
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'test/binary-topic') {
                receivedSelector = selector;
                receivedBinary = payload;
                console.log(`[Test] JS Node received notification for ${selector.path}, payload size: ${payload?.length} bytes`);
            }
        });

        await nodeVFS.subscribe(topic);
        console.log('✔ JS Node subscribed to test/binary-topic');

        // 4. Trigger Binary Notification on Export Node
        // We simulate an incoming binary notification (e.g. from an ESP32)
        // by POSTing to the export node's /notify endpoint with X-VFS-Encoding: bytes
        console.log('[Test] Triggering binary notification on Export Node...');
        const binaryData = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x42, 0xFF]);
        
        const body = encodeRecord({
            op: 'PUB',
            selector: topic.toJSON(),
            encoding: 'bytes'
        }, binaryData);

        const notifyRes = await fetch(`${protocol}://localhost:${EXPORT_PORT}/notify`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/octet-stream'
            },
            body: body
        });
        
        assert.strictEqual(notifyRes.status, 200, 'C++ node should accept binary /notify');
        console.log('✔ Binary notification triggered on C++ Node');

        // 5. Verify JS Node received the exact binary payload
        console.log('[Test] Waiting for JS Node to receive binary notification...');
        let attempts = 0;
        while (!receivedBinary && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(receivedBinary, 'JS Node should have received the notification');
        assert.ok(receivedBinary instanceof Uint8Array, 'Payload should be a Uint8Array');
        assert.deepStrictEqual(Array.from(receivedBinary), Array.from(binaryData), 'Binary payload must match exactly');
        console.log('✔ JS Node received correct binary data from C++ forwarding!');

    } finally {
        mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
