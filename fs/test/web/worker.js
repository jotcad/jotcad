import { VFS, IndexedDBStorage } from '/src/vfs_browser.js';
import { SyncVFSAtomics } from '/src/sync_vfs_atomics.js';

const storage = new IndexedDBStorage();
const vfs = new VFS({ id: 'worker', storage });

let syncVFS = null;

const browserPeer = {
    send: (event) => self.postMessage(event)
};
vfs.connect(browserPeer);

self.onmessage = (e) => {
    if (e.data.type === 'init-sync') {
        syncVFS = new SyncVFSAtomics(e.data.sab);
        console.log('Worker SyncVFS ready.');
        return;
    }
    vfs.receive(e.data, browserPeer);
};

async function run() {
    console.log('Worker VFS starting...');
    
    // Test Scenario:
    // Worker watches for demand on its output
    const watcher = vfs.watch('worker-done', { states: ['PENDING'] });
    for await (const event of watcher) {
        console.log('Worker saw demand for worker-done!');
        
        // Use Synchronous Lease
        const hasLease = await vfs.lease('worker-done', 5000);
        if (hasLease) {
            console.log('Worker leased worker-done, performing SYNCHRONOUS write...');
            
            if (syncVFS) {
                // Perform a SYNCHRONOUS write via Atomics
                const data = new TextEncoder().encode('Hello from Worker (Sync)');
                syncVFS.write('worker-done', {}, data);
                console.log('Worker synchronous write complete!');
            } else {
                // Fallback to async if sync not ready
                const stream = new ReadableStream({
                    start(controller) {
                        controller.enqueue(new TextEncoder().encode('Hello from Worker (Async Fallback)'));
                        controller.close();
                    }
                });
                await vfs.write('worker-done', {}, stream);
            }
        }
    }
}

run();
