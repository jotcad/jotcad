import http from 'node:http';
import { VFS, MeshLink, registerVFSRoutes, MemoryStorage } from './fs/src/index.js';

async function main() {
    const vfsA = new VFS({ id: 'node-A', storage: new MemoryStorage() });
    const vfsB = new VFS({ id: 'node-B', storage: new MemoryStorage() });

    // Node B provides some data
    vfsB.registerProvider('test/remote', async () => new TextEncoder().encode('remote-success'));

    const meshA = new MeshLink(vfsA, ['http://localhost:26002'], { localUrl: 'http://localhost:26001' });
    const meshB = new MeshLink(vfsB, ['http://localhost:26001'], { localUrl: 'http://localhost:26002' });

    const serverA = http.createServer(); registerVFSRoutes(vfsA, serverA, '', meshA); serverA.listen(26001);
    const serverB = http.createServer(); registerVFSRoutes(vfsB, serverB, '', meshB); serverB.listen(26002);

    await vfsA.init(); await vfsB.init();
    await meshA.start(); await meshB.start();

    // Wait for peering
    await new Promise(r => setTimeout(r, 1000));

    console.log('--- Requesting Remote Data from Node A ---');
    try {
        const result = await vfsA.readText('test/remote', {});
        console.log('RESULT:', result);
    } catch (e) {
        console.error('ERROR:', e);
    }

    serverA.close(); serverB.close();
    meshA.stop(); meshB.stop();
    await vfsA.close(); await vfsB.close();
}
main();
