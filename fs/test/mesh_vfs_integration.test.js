import { describe, it, expect, beforeAll, afterAll } from 'vitest';
import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { existsSync } from 'node:fs';

const TEST_STORAGE_A = path.join(process.cwd(), '.test_vfs_a');
const TEST_STORAGE_B = path.join(process.cwd(), '.test_vfs_b');
const TEST_STORAGE_C = path.join(process.cwd(), '.test_vfs_c');
const TEST_STORAGE_D = path.join(process.cwd(), '.test_vfs_d');

async function createNode(id, port, neighbors = [], storageDir) {
    const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
    const localUrl = `http://localhost:${port}/vfs`;
    const mesh = new MeshLink(vfs, neighbors, { localUrl });
    const server = http.createServer();
    registerVFSRoutes(vfs, server, '/vfs', mesh);
    await new Promise(resolve => server.listen(port, '0.0.0.0', resolve));
    await vfs.init();
    await mesh.start();
    return { vfs, mesh, server, port, id, storageDir, localUrl };
}

describe('Decentralized Mesh-VFS Integration', () => {
    let nodeA, nodeB, nodeC;

    beforeAll(async () => {
        nodeC = await createNode('node-c', 9102, [], TEST_STORAGE_C);
        nodeB = await createNode('node-b', 9101, [nodeC.localUrl], TEST_STORAGE_B);
        nodeA = await createNode('node-a', 9100, [nodeB.localUrl], TEST_STORAGE_A);
    });

    afterAll(async () => {
        await nodeA.server.close();
        await nodeB.server.close();
        await nodeC.server.close();
        await nodeA.vfs.close();
        await nodeB.vfs.close();
        await nodeC.vfs.close();
        await fs.rm(TEST_STORAGE_A, { recursive: true, force: true });
        await fs.rm(TEST_STORAGE_B, { recursive: true, force: true });
        await fs.rm(TEST_STORAGE_C, { recursive: true, force: true });
        await fs.rm(TEST_STORAGE_D, { recursive: true, force: true }).catch(() => {});
    });

    it('should implement Ephemeral Wipe on startup', async () => {
        const dummyFile = path.join(TEST_STORAGE_C, 'ghost.meta');
        await fs.writeFile(dummyFile, 'phantom data');
        expect(existsSync(dummyFile)).toBe(true);
        await nodeC.vfs.close();
        nodeC.server.close();
        nodeC = await createNode('node-c', 9102, [], TEST_STORAGE_C);
        expect(existsSync(dummyFile)).toBe(false);
    });

    it('should fulfill a recursive Bread-crumb READ (A -> B -> C)', async () => {
        const vfsPath = 'mesh/integration/test';
        const vfsParams = { version: '1.0.0' };
        const vfsContent = JSON.stringify({ message: 'Hello from the far end!' });
        await nodeC.vfs.writeData(vfsPath, vfsParams, vfsContent);
        
        console.log('[Test] Triggering recursive READ from Node A...');
        const result = await nodeA.vfs.readData(vfsPath, vfsParams);
        expect(result).toEqual({ message: 'Hello from the far end!' });
        console.log('[Test] READ SUCCESSful.');
    }, 15000);

    it('should implement Auto-Peering (Symmetry)', async () => {
        const nodeD = await createNode('node-d', 9103, [nodeC.localUrl], TEST_STORAGE_D);
        expect(nodeC.mesh.neighbors.has(nodeD.localUrl)).toBe(false);

        const path = 'mesh/peering/test';
        await nodeC.vfs.writeData(path, {}, "peering check");
        await nodeD.vfs.readData(path, {});

        expect(nodeC.mesh.neighbors.has(nodeD.localUrl)).toBe(true);
        console.log('[Test] AUTO-PEERING SUCCESSful.');
        
        await nodeD.vfs.close();
        await nodeD.server.close();
    });
});
