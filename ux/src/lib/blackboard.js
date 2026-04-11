import { createSignal } from 'solid-js';
import { VFS, IndexedDBStorage } from '../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../fs/src/vfs_rest_bridge_browser.js';

/**
 * The Global VFS instance for the UI.
 * Linked to the shared Origin IndexedDB and bridged to the Export Service (9092).
 */
export const vfs = new VFS({ 
    id: 'ui-main',
    storage: new IndexedDBStorage()
});

// Use the environment variable VITE_VFS_URL or fall back to the Export Peer (9092)
const vfsUrl = import.meta.env.VITE_VFS_URL || `http://${window.location.hostname}:9092/vfs`;
const mesh = new MeshLink(vfs, [vfsUrl]);

// Reactive graph state for Solid.js
const [graph, setGraph] = createSignal({});

export const blackboard = {
    vfs,
    graph,
    async start() {
        console.log(`[UX] Mesh-VFS initialized. Connecting to nearest peer: ${vfsUrl}`);
        
        // In the Silent Mesh, we don't have a proactive SSE sync.
        // We only update the local graph when we perform local VFS actions.
        vfs.events.on('state', async (event) => {
            if (!event.path) return;
            console.log(`[UX] Local state change: ${event.path} -> ${event.state}`);
            
            setGraph(prev => ({
                ...prev,
                [event.cid]: { ...prev[event.cid], ...event }
            }));
        });

        await mesh.start();
        
        // Initial Discovery (Optional): We could ask for /peers here if we wanted to crawl.
    },
    stop() {
        mesh.stop();
        vfs.close();
    },
    async read(path, parameters = {}) {
        return vfs.read(path, parameters);
    },
    async write(path, parameters = {}, data) {
        return vfs.writeData(path, parameters, data);
    }
};

if (typeof window !== 'undefined') {
    window.blackboard = blackboard;
}
