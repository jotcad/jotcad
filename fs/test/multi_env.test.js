import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';

test('Multi-Environment Mesh Coordination', async (t) => {
    let nodeNode, browserVfs, nodeServer, stopServer;
    const storageDir = path.resolve('.test_vfs_multi_env');

    t.after(async () => {
        if (nodeNode) nodeNode.mesh.stop();
        if (stopServer) stopServer();
        if (nodeNode) await nodeNode.vfs.close();
        if (nodeServer) nodeServer.close();
        if (browserVfs) await browserVfs.close();
        await fs.rm(storageDir, { recursive: true, force: true }).catch(() => {});
    });

    // 1. Setup "Node" Node (Server)
    const vfsNode = new VFS({ id: 'node-env', storage: new DiskStorage(storageDir) });
    const meshNode = new MeshLink(vfsNode, [], { localUrl: 'http://localhost:9300' });
    nodeServer = http.createServer();
    stopServer = registerVFSRoutes(vfsNode, nodeServer, '', meshNode);
    await new Promise(resolve => nodeServer.listen(9300, '0.0.0.0', resolve));
    await vfsNode.init();
    await meshNode.start();
    nodeNode = { vfs: vfsNode, mesh: meshNode };

    // 2. Setup "Browser" Node (Client)
    // In this test, we simulate the browser by just using a VFS with MemoryStorage
    browserVfs = new VFS({ id: 'browser-env' });
    const meshBrowser = new MeshLink(browserVfs, ['http://localhost:9300']);
    await browserVfs.init();
    await meshBrowser.start();

    await t.test('browser can fetch data provisioned by node', async () => {
        const p = 'env/sharing/test';
        const data = { msg: 'from node to simulated browser' };
        
        await vfsNode.writeData(p, {}, data);
        const result = await browserVfs.readData(p, {});
        
        assert.deepStrictEqual(result, data);
    });

    meshBrowser.stop();
});
