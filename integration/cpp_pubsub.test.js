import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import fs from 'node:fs';
import path from 'node:path';

// Helper to wait for a specific log message from a process
function waitForLog(proc, pattern, timeout = 5000) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(new Error(`Timeout waiting for log: ${pattern}`)), timeout);
        const onData = (data) => {
            const str = data.toString();
            if (str.match(pattern)) {
                proc.stdout.off('data', onData);
                proc.stderr.off('data', onData);
                clearTimeout(timer);
                resolve(str);
            }
        };
        proc.stdout.on('data', onData);
        proc.stderr.on('data', onData);
    });
}

test('Node.js <-> C++ Pub-Sub Integration', async (t) => {
    const CPP_PORT = 9095;
    const STORAGE_DIR = './.test_cpp_integration';
    const BIN_PATH = './geo/bin/ops'; // Use the consolidated ops binary

    if (!fs.existsSync(BIN_PATH)) {
        throw new Error(`C++ Test Server not found at ${BIN_PATH}. Please run './build.sh' first.`);
    }

    if (fs.existsSync(STORAGE_DIR)) fs.rmSync(STORAGE_DIR, { recursive: true });
    fs.mkdirSync(STORAGE_DIR);

    // Initialize VFS & Mesh early for cleanup visibility
    const nodeVFS = new VFS({ id: 'node-js' });
    const mesh = new MeshLink(nodeVFS);

    console.log('[Test] Spawning C++ Node on port', CPP_PORT);
    const cppNode = spawn(BIN_PATH, [
        '--id', 'cpp-node',
        '--port', CPP_PORT.toString(),
        '--storage', STORAGE_DIR
    ]);

    // Cleanup logic: Stop mesh (abort fetches) THEN kill process
    const cleanup = () => {
        mesh.stop();
        cppNode.kill();
        if (fs.existsSync(STORAGE_DIR)) fs.rmSync(STORAGE_DIR, { recursive: true });
    };
    process.on('exit', cleanup);

    try {
        await waitForLog(cppNode, /Internal server listening/);

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
        await waitForLog(cppNode, /subscribe: test\/topic/);
        console.log('✔ C++ received subscription interest');

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
            if (selector.path === 'sys/schema' && payload.type === 'CATALOG_ANNOUNCEMENT') {
                catalogReceived = payload;
            }
        });

        await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);
        
        attempts = 0;
        while (!catalogReceived && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(catalogReceived, 'Node.js should have received CATALOG_ANNOUNCEMENT');
        assert.strictEqual(catalogReceived.provider, 'cpp-node');
        console.log('✔ Node.js received Catalog from C++');

    } finally {
        cleanup();
    }
});
