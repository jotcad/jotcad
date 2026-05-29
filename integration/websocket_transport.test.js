import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { VFS } from '../fs/src/vfs.js';
import { MeshLink, ReadSelectorRequest } from '../fs/src/mesh_link.js';
import { Selector } from '../fs/src/cid.js';
import { launchSystem } from '../orchestrator.js';

test('VFS WebSocket Transport: Negotiation, Upgrade, and Fallback Recovery', async (t) => {
    // instruct JS nodes to clean storage on initialization
    process.env.VFS_EPHEMERAL_WIPE = 'true';

    // Manually clean up any stale storage directories before starting
    const __dirname = path.resolve();
    const staleDirs = [
        path.join(__dirname, '.vfs_storage_node_a'),
        path.join(__dirname, '.vfs_storage_node_b'),
        path.join(__dirname, '.vfs_storage_test_sovereign_js_node_a'),
        path.join(__dirname, '.vfs_storage_test_sovereign_js_node_b')
    ];
    for (const dir of staleDirs) {
        if (fs.existsSync(dir)) {
            fs.rmSync(dir, { recursive: true, force: true });
        }
    }

    console.log('[Test WS VFS] Launching sovereign JS mesh cluster...');
    const sys = await launchSystem('test/sovereign_js', 'DEBUG');
    const PORT = sys.ports.node_a;

    // Wait for Node A to be healthy
    let ready = false;
    for (let i = 0; i < 50; i++) {
        try {
            const resp = await fetch(`http://localhost:${PORT}/health`);
            if (resp.ok) { ready = true; break; }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 100));
    }
    assert.ok(ready, 'Server Node A failed to start.');

    // Wait for mesh stabilization (discovery + upgrades)
    await new Promise(r => setTimeout(r, 2000));

    // 1. Initialise WS-capable subscriber client
    const wsNodeVFS = new VFS({ id: 'node-ws-subscriber' });
    const wsMesh = new MeshLink(wsNodeVFS, [], { localUrl: 'http://localhost:8189' });
    await wsNodeVFS.init();

    // Register local mock provider on WS subscriber
    wsNodeVFS.registerProvider('subscriber/data', async (vfs, selector, context) => {
        const bytes = new Uint8Array([100, 101, 102]); // 'efg'
        return {
            stream: new ReadableStream({
                start(controller) {
                    controller.enqueue(bytes);
                    controller.close();
                }
            }),
            metadata: { state: 'AVAILABLE', encoding: 'bytes' }
        };
    });

    // 2. Initialise HTTP-only subscriber client (Simulates WS degradation)
    const httpNodeVFS = new VFS({ id: 'node-http-subscriber' });
    const httpMesh = new MeshLink(httpNodeVFS, [], { fetch: globalThis.fetch.bind(globalThis) });
    // Strip transports to simulate a pure HTTP client
    httpMesh.neighborUrls = []; 
    await httpNodeVFS.init();

    let brokenMesh = null;
    try {
        // --- TEST 1: WebSocket Handshake & Negotiation Upgrade ---
        console.log('[Test WS VFS] Connecting WS subscriber to Server Node A...');
        await wsMesh.addPeer(`http://localhost:${PORT}`);

        const wsConn = [...wsMesh.peers.values()].find(c => c.neighborId.endsWith('node_a'));
        assert.ok(wsConn, 'WebSocket client should be connected to Node A.');
        assert.equal(wsConn.reachability, 'DIRECT');
        assert.equal(
            wsConn.constructor.name, 
            'WSNodeForwardConnection', 
            'Connection should be upgraded to WSNodeForwardConnection.'
        );
        console.log('✔ WebSocket direct transport upgraded and active.');

        // --- TEST 2: Fallback Recovery on Socket Failure ---
        console.log('[Test WS VFS] Verifying fallback recovery to HTTP when WS upgrade fails...');
        brokenMesh = new MeshLink(httpNodeVFS, [], { localUrl: 'http://localhost:8199' });
        const originalFetch = brokenMesh.fetch;
        brokenMesh.fetch = async (url, opts) => {
            const resp = await originalFetch(url, opts);
            if (url.endsWith('/register')) {
                const json = await resp.clone().json();
                return {
                    ok: true,
                    status: 200,
                    json: async () => ({
                        ...json,
                        wsUrl: 'ws://localhost:9999/vfs-ws' // Unreachable port to force WS failure
                    })
                };
            }
            return resp;
        };
        await brokenMesh.addPeer(`http://localhost:${PORT}`);
        const fallbackConn = [...brokenMesh.peers.values()].find(c => c.neighborId.endsWith('node_a'));
        assert.ok(fallbackConn, 'Should fall back and register Node A.');
        assert.equal(
            fallbackConn.constructor.name, 
            'ForwardConnection', 
            'Should fall back to standard HTTP ForwardConnection.'
        );
        console.log('✔ Fallback successfully degraded to HTTP direct.');

        // --- TEST 3: Bidirectional Forward Read over WebSocket ---
        console.log('[Test WS VFS] Resolving VFS operations over Forward WebSocket...');
        // Node A registers several geometric primitives. Let's read one.
        const boxSelector = Selector.fromObject({
            path: 'jot/Box',
            parameters: { width: 10, height: 10, depth: 10 },
            output: '$out'
        });
        const readResult = await wsConn.send(new ReadSelectorRequest(boxSelector));
        assert.ok(readResult && readResult.body, 'Forward VFS read over WS should return a stream.');
        console.log('✔ Read request successfully completed over WebSocket.');

        // --- TEST 4: Reverse VFS reads via Server Push ---
        console.log('[Test WS VFS] Triggering server-push Reverse VFS read...');
        // We make a REST call to Node A asking it to resolve a resource owned by our WS subscriber.
        // Node A must push this VFS request down the active WS socket to the client, receive the
        // binary reply, and stream it back over REST.
        const gatewayReadResp = await fetch(`http://localhost:${PORT}/read_selector`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                selector: { path: 'subscriber/data', parameters: {} },
                stack: []
            })
        });
        assert.ok(gatewayReadResp.ok, 'Gateway node failed to resolve reverse WS resource.');
        const arrayBuf = await gatewayReadResp.arrayBuffer();
        const outputBytes = new Uint8Array(arrayBuf);
        assert.equal(outputBytes.length, 3);
        assert.deepEqual(outputBytes, new Uint8Array([100, 101, 102]));
        console.log('✔ Reverse VFS read over persistent WebSocket tunnel successfully verified!');

    } finally {
        console.log('[Test WS VFS] Tearing down resources...');
        wsMesh.stop();
        await wsNodeVFS.close();

        httpMesh.stop();
        await httpNodeVFS.close();

        if (brokenMesh) brokenMesh.stop();

        await sys.stop();
    }
});
