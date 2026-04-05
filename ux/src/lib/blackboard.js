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
        vfs.events.on('state', async (event) => {
            if (!event.path) return; // Ignore non-coordinate events (e.g. CLOSED)

            const localCid = await vfs.getCID({ path: event.path, parameters: event.parameters });
            if (localCid !== event.cid) {
                console.error('CID MISMATCH DETECTED!', {
                    path: event.path,
                    params: event.parameters,
                    hubCid: event.cid,
                    browserCid: localCid
                });
                throw new Error(`CID Mismatch: Peer sent ${event.cid} but we calculated ${localCid} for ${event.path}`);
            }

            console.log(`[UX] Received state event: ${event.path} CID: ${event.cid} Source: ${event.source}`);
            
            let data = event.data;
            if (event.state === 'SCHEMA' && data) {
                try {
                    const text = new TextDecoder().decode(data);
                    data = JSON.parse(text);
                } catch (e) {
                    console.warn('[UX] Failed to decode schema data', e);
                }
            }

            setGraph(prev => ({
                ...prev,
                [event.cid]: { ...prev[event.cid], ...event, data }
            }));
        });

        await bridge.start();

        // One-time initial pull of states after bridge is up
        const resp = await fetch(`http://${window.location.hostname}:9090/vfs/states`);
        if (resp.ok) {
            const states = await resp.json();
            console.log('[UX] Initial states from hub:', states);
            
            for (const s of states) {
                if (!s.path) continue; // Skip malformed states (e.g. key: undefined)

                const localCid = await vfs.getCID({ path: s.path, parameters: s.parameters });
                if (localCid !== s.cid) {
                    console.error('CID MISMATCH IN INITIAL SYNC!', {
                        path: s.path,
                        params: s.parameters,
                        hubCid: s.cid,
                        browserCid: localCid
                    });
                    throw new Error(`CID Mismatch in initial sync: Hub has ${s.cid} but browser expects ${localCid}`);
                }

                let data = s.data;
                if (s.state === 'SCHEMA' && data) {
                    try {
                        // Data from fetch/json is usually base64 or array of numbers
                        // But since we use JSON.stringify on Buffer in server, it might be {type: 'Buffer', data: [...]}
                        const bytes = data.data ? new Uint8Array(data.data) : new Uint8Array(data);
                        const text = new TextDecoder().decode(bytes);
                        data = JSON.parse(text);
                    } catch (e) {
                        console.warn('[UX] Failed to decode initial schema data', e);
                    }
                }

                setGraph(prev => ({
                    ...prev,
                    [s.cid]: { ...prev[s.cid], ...s, data, source: 'remote' }
                }));
            }
        }
    },
    stop() {
        bridge.stop();
        vfs.close();
    },
    tickle(path, parameters = {}) {
        return vfs.tickle(path, parameters);
    },
    declare(path, schema) {
        return vfs.declare(path, schema);
    }
};

if (typeof window !== 'undefined') {
    window.blackboard = blackboard;
}
