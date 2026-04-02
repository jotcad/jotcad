import { VFS, IndexedDBStorage } from '/src/vfs_browser.js';
import { RESTBridge } from '/src/vfs_rest_bridge.js';
import { SyncVFSServer } from '/src/sync_vfs_atomics.js';

async function run() {
    const storage = new IndexedDBStorage();
    const vfs = new VFS({ id: 'browser', storage });

    // 1. Setup Sync Server for Workers
    const sab = new SharedArrayBuffer(1024 * 1024);
    const syncServer = new SyncVFSServer(vfs, sab);
    syncServer.start(1);

    const bridge = new RESTBridge(vfs, `${location.origin}/vfs`);
    await bridge.start();

    const worker = new Worker('worker.js', { type: 'module' });
    const workerPeer = { send: (event) => worker.postMessage(event) };
    vfs.connect(workerPeer);
    worker.onmessage = (e) => vfs.receive(e.data, workerPeer);
    
    // Pass the SAB to the worker
    worker.postMessage({ type: 'init-sync', sab });

    if ('serviceWorker' in navigator) {
        const registration = await navigator.serviceWorker.register('service-worker.js', { type: 'module' });
        await navigator.serviceWorker.ready;
        if (!navigator.serviceWorker.controller) {
            location.reload();
            return;
        }
        const swPeer = {
            send: (event) => {
                if (navigator.serviceWorker.controller) {
                    navigator.serviceWorker.controller.postMessage(event);
                }
            }
        };
        vfs.connect(swPeer);
        navigator.serviceWorker.addEventListener('message', (e) => vfs.receive(e.data, swPeer));
    }

    document.getElementById('status').textContent = 'Ready';

    const watcher = vfs.watch('final-result');
    for await (const event of watcher) {
        if (event.state === 'AVAILABLE') {
            document.getElementById('status').textContent = 'Test Passed';
            break;
        }
    }
}

run().catch(err => {
    console.error(err);
    document.getElementById('status').textContent = 'Test Failed';
});
