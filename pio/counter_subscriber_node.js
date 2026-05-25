import http from 'node:http';
import path from 'node:path';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes, Selector, log } from '../fs/src/index.js';

const id = process.env.VFS_ID || 'counter-subscriber';
const port = parseInt(process.env.PORT || '11223');
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const storageDir = path.resolve(`.vfs_storage_${id}`);

log(`[Subscriber ${id}] Initializing Counter Subscriber Node...`);

const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

log(`[Subscriber ${id}] Connecting to cluster neighbors: [${neighbors.join(', ')}]`);
const meshLink = new MeshLink(vfs, neighbors, { localUrl: `http://localhost:${port}` });

/**
 * Handle Sensor Data: 
 * This node specifically cares about the counter reported by the ESP32.
 */
vfs.events.on('notify', (selector, payload) => {
    if (selector.path === 'sensor/counter') {
        log(`[Subscriber ${id}] 📈 COUNTER UPDATE: ${payload.value} (via ${selector.path})`);
    }
});

/**
 * Active Health Check:
 * Prove the reverse tunnel is open by reaching back into the ESP32 every 10 seconds.
 */
setInterval(async () => {
    try {
        // Find if we have an ESP32 reverse peer connected
        const hasEsp32 = Array.from(meshLink.peers.keys()).some(p => p.includes('esp32'));
        
        if (hasEsp32) {
            const result = await vfs.read(Selector.fromObject({ path: 'sensor/counter' }));
            log(`[Subscriber ${id}] 🔍 Active Read-back: Current counter value is ${result.value}`);
        }
    } catch (err) {
        // Device likely polling or offline; suppress errors to keep logs clean
    }
}, 10000);

const server = http.createServer();
registerVFSRoutes(vfs, server, '', meshLink);

server.listen(port, '0.0.0.0', async () => {
    log(`[Subscriber ${id}] Listening for ESP32 on http://0.0.0.0:${port}`);
    await meshLink.start();

    // Protocol: Explicit Interest Registration
    log(`[Subscriber ${id}] Subscribing to sensor/counter mesh updates...`);
    meshLink.subscribe(Selector.fromObject({ path: 'sensor/counter' }), Date.now() + 1000 * 60 * 60);
});
