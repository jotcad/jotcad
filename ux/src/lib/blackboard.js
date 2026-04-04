import { createSignal } from 'solid-js';
import { VFS, IndexedDBStorage } from '../../../fs/src/vfs_browser.js';
import { RESTBridge } from '../../../fs/src/vfs_rest_bridge_browser.js';

/**
 * The Global VFS instance for the UI.
 * Linked to the shared Origin IndexedDB and bridged to the Node.js Hub.
 */
export const vfs = new VFS({ 
    id: 'ui-main',
    storage: new IndexedDBStorage()
});

const bridge = new RESTBridge(vfs, `http://${window.location.hostname}:9090/vfs`);

// Reactive graph state for Solid.js
const [graph, setGraph] = createSignal({});

export const blackboard = {
    vfs,
    graph,
    async start() {
        // Sync local graph with VFS state changes
        vfs.events.on('state', (event) => {
            setGraph(prev => ({
                ...prev,
                [event.cid]: { ...prev[event.cid], ...event }
            }));
        });

        await bridge.start();
    },
    stop() {
        bridge.stop();
        vfs.close();
    },
    tickle(path, parameters = {}) {
        return vfs.tickle(path, parameters);
    }
};
