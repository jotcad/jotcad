import http from 'node:http';
import { VFS } from '../../fs/src/vfs_node.js';
import { Selector } from '../../fs/src/cid.js';
import { MeshLink } from '../../fs/src/mesh_link.js';
import { registerVFSRoutes } from '../../fs/src/vfs_rest_server.js';
import { log } from '../../fs/src/log.js';

async function startMeshNode({ id, port, storageDir }) {
    const vfs = new VFS({ id });
    vfs.storage.root = storageDir;
    await vfs.init();
    
    const mesh = new MeshLink(vfs);
    await mesh.start();
    
    const server = http.createServer((req, res) => {
        log(`[Test JS Neighbor] Incoming ${req.method} ${req.url} from ${req.socket.remoteAddress}`);
    });

    server.on('connection', (socket) => {
        log(`[Test JS Neighbor] NEW TCP CONNECTION from ${socket.remoteAddress}:${socket.remotePort}`);
        socket.on('error', (err) => log(`[Test JS Neighbor] Socket Error: ${err.message}`));
    });

    const unregister = registerVFSRoutes(vfs, server, '', mesh);
    
    await new Promise(resolve => server.listen(port, '0.0.0.0', resolve));
    
    log(`[Test] JS Neighbor listening on http://0.0.0.0:${port} (ID: ${id})`);

    // Listen for notifications from the mesh
    vfs.events.on('notify', (selector, payload) => {
        log(`[MESH EVENT] Notification from ESP32: ${selector.path} -> ${JSON.stringify(payload)}`);
    });

    // PERIODIC REMOTE READ: Prove we can talk BACK to the ESP32
    setInterval(async () => {
        try {
            log(`[Test] Requesting remote READ from ESP32...`);
            const result = await vfs.read(Selector.fromObject({ path: 'sensor/counter' }));
            log(`[Test] REMOTE READ SUCCESS: Counter is ${result.value}`);
        } catch (err) {
            log(`[Test] REMOTE READ FAILED: ${err.message}`);
        }
    }, 5000);

    return {
        vfs,
        mesh,
        server,
        stop: async () => {
            unregister();
            mesh.stop();
            await vfs.close();
            await new Promise(resolve => server.close(resolve));
        }
    };
}

const node = await startMeshNode({
    id: 'js-test-neighbor',
    port: 11223,
    storageDir: '.vfs_storage_real_device'
});

process.on('SIGINT', async () => {
    log('\nShutting down...');
    await node.stop();
    process.exit();
});
