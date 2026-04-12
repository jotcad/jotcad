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
    await new Promise(resolve => server.listen(port, '127.0.0.1', resolve));
    await vfs.init();
    return { vfs, mesh, server, port, id, localUrl: url, stopServer };
}

test('Mesh Handshake & Reachability Negotiation', { timeout: 5000 }, async (t) => {
    let nodeA, nodeB;

    t.after(async () => {
        if (nodeA) { nodeA.mesh.stop(); nodeA.stopServer(); nodeA.server.close(); await nodeA.vfs.close(); }
        if (nodeB) { nodeB.mesh.stop(); nodeB.stopServer(); nodeB.server.close(); await nodeB.vfs.close(); }
        await fs.rm(STORAGE_A, { recursive: true, force: true }).catch(() => {});
        await fs.rm(STORAGE_B, { recursive: true, force: true }).catch(() => {});
    });

    await t.test('should negotiate DIRECT linkage on localhost', async () => {
        nodeB = await createNode('b', 9201);
        nodeA = await createNode('a', 9200);

        console.log('[Test Handshake] Starting handshake from A to B...');
        const remoteId = await nodeA.mesh.addPeer(nodeB.localUrl);
        
        assert.strictEqual(remoteId, 'b');

        // Wait for symmetric registration to complete (it is backgrounded)
        for (let i = 0; i < 20; i++) {
            if (nodeB.mesh.peers.has('a')) break;
            await new Promise(r => setTimeout(r, 50));
        }
        
        assert.ok(nodeB.mesh.peers.has('a'), 'Node B should have registered Node A symmetrically');
        assert.strictEqual(nodeB.mesh.reverseRegistry.listeners.has('a'), false, 'Should NOT have a reverse listener for direct peer');
    });

    await t.test('should negotiate REVERSE fallback when direct probe fails', async () => {
        if (nodeA) { nodeA.stopServer(); nodeA.server.close(); await nodeA.vfs.close(); }
        if (nodeB) { nodeB.stopServer(); nodeB.server.close(); await nodeB.vfs.close(); }
        
        nodeB = await createNode('b', 9201);
        nodeA = await createNode('a', 9200, [], 'http://127.0.0.1:9999/broken');

        console.log('[Test Handshake] Starting handshake with bogus URL...');
        await nodeA.mesh.addPeer(nodeB.localUrl);

        for (let i = 0; i < 20; i++) {
            if (nodeB.mesh.reverseRegistry.listeners.has('a')) break;
            await new Promise(r => setTimeout(r, 50));
        }

        assert.ok(nodeB.mesh.reverseRegistry.listeners.has('a'), 'Node B SHOULD have a reverse listener for unreachable peer');
        console.log('[Test Handshake] REVERSE fallback SUCCESSful.');
    });

    await t.test('should be idempotent (multiple addPeer calls)', async () => {
        if (nodeA) { nodeA.stopServer(); nodeA.server.close(); await nodeA.vfs.close(); }
        if (nodeB) { nodeB.stopServer(); nodeB.server.close(); await nodeB.vfs.close(); }
        
        nodeB = await createNode('b', 9201);
        nodeA = await createNode('a', 9200);

        let registerCalls = 0;
        const originalFetch = nodeA.mesh.fetch;
        nodeA.mesh.fetch = async (url, options) => {
            if (url.endsWith('/register')) registerCalls++;
            return originalFetch(url, options);
        };

        console.log('[Test Handshake] Triggering 10 parallel addPeer calls...');
        await Promise.all(Array(10).fill(0).map(() => nodeA.mesh.addPeer(nodeB.localUrl)));

        assert.strictEqual(registerCalls, 1, 'Should only call /register ONCE for the same URL');
        console.log('[Test Handshake] Idempotency SUCCESSful.');
    });
});
