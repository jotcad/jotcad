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

runIntegrationTest('C++ -> Node.js Binary Notification Forwarding', async ({ vfs, sys }) => {
    const EXPORT_PORT = sys.ports.export;

    const hasCerts = fs.existsSync(path.join(__dirname, '../.ssl/localhost-key.pem')) && 
                     fs.existsSync(path.join(__dirname, '../.ssl/localhost-cert.pem'));
    const protocol = hasCerts ? 'https' : 'http';

    // 3. Subscribe JS Node to a binary topic
    const topic = new Selector('test/binary-topic');
    let receivedBinary = null;
    let receivedSelector = null;
    
    vfs.events.on('notify', (selector, payload) => {
        if (selector.path === 'test/binary-topic') {
            receivedSelector = selector;
            receivedBinary = payload;
            console.log(`[Test] JS Node received notification for ${selector.path}, payload size: ${payload?.length} bytes`);
        }
    });

    await vfs.subscribe(topic);
    console.log('✔ JS Node subscribed to test/binary-topic');

    // 4. Trigger Binary Notification on Export Node
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
});
