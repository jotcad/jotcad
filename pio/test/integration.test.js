import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { spawn } from 'node:child_process';
import { VFS } from '../../fs/src/vfs_node.js';
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

test('ESP32 VFS Integration: Subscription & Notification', { timeout: 60000 }, async (t) => {
    // 1. Start a local JS VFS Node to act as the neighbor
    const node = await startMeshNode({
        id: 'js-test-neighbor',
        port: 11223,
        storageDir: '.vfs_storage_pio_test'
    });
    
    log(`[Test] JS Neighbor listening on http://0.0.0.0:11223 (ID: js-test-neighbor)`);

    // 2. Start the Wokwi Simulation
    // We explicitly pass WOKWI_GATEWAY to ensure the CLI connects to our local bridge
    const pio = spawn('export $(grep -v "^#" .env | xargs) && pio run -d pio && wokwi-cli pio/', {
        shell: true,
        stdio: ['inherit', 'pipe', 'pipe'],
        env: { 
            ...process.env,
            WOKWI_GATEWAY: 'ws://127.0.0.1:9011'
        }
    });

    let pioReady = false;
    const notifications = [];

    // Log PIO output for debugging
    pio.stdout.on('data', (data) => {
        const str = data.toString().trim();
        if (str) log(`[PIO STDOUT] ${str}`);
        if (str.includes('VFS_READY')) {
            pioReady = true;
            log('[Test] ESP32 reported VFS_READY');
        }
    });

    pio.stderr.on('data', (data) => {
        log(`[PIO STDERR] ${data.toString().trim()}`);
    });

    // Listen for notifications from the ESP32
    node.vfs.events.on('notify', (selector, payload) => {
        log(`[Test] Received Notification from mesh: ${selector.path} -> ${JSON.stringify(payload)}`);
        if (selector.path === 'sensor/counter') {
            notifications.push(payload.value);
            log(`[Test] Recorded counter update #${notifications.length}: ${payload.value}`);
        }
    });

    try {
        // 3. Wait for ESP32 to boot and register
        await new Promise((resolve, reject) => {
            const testTimeout = setTimeout(() => {
                clearInterval(checkInterval);
                reject(new Error(`Test Timed Out. Received ${notifications.length} notifications.`));
            }, 55000);
            
            // Check if we received counter updates
            const checkInterval = setInterval(() => {
                if (notifications.length >= 3) {
                    clearInterval(checkInterval);
                    clearTimeout(testTimeout);
                    resolve();
                }
            }, 500);
        });

        assert.ok(notifications.length >= 3, 'Should receive at least 3 counter updates');
        log('[Test] SUCCESS: Received targeted notifications from ESP32.');

    } catch (err) {
        log(`[Test] FAILED: ${err.message}`);
        throw err;
    } finally {
        log('[Test] Cleaning up...');
        pio.kill('SIGTERM');
        // Force kill if it doesn't exit
        const forceKill = setTimeout(() => pio.kill('SIGKILL'), 2000);
        await node.stop();
        clearTimeout(forceKill);
        log('[Test] Cleanup complete.');
    }
});
