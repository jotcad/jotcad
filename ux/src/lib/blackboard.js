import { createSignal } from 'solid-js';
import { VFS, IndexedDBStorage } from '../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../jot/src/index.js';

/**
 * The Global VFS instance for the UI.
 * Linked to the shared Origin IndexedDB and bridged to the Export Service (9092).
 */
export const vfs = new VFS({ 
    id: 'ui-main',
    storage: new IndexedDBStorage()
});

registerJotProvider(vfs);

// Use the environment variable VITE_VFS_URL or fall back to the Export Peer (9092)
const vfsUrl = import.meta.env.VITE_VFS_URL || `http://${window.location.hostname}:9092`;
const mesh = new MeshLink(vfs, [vfsUrl]);

// Reactive graph state for Solid.js
const [graph, setGraph] = createSignal({});
const [schemas, setSchemas] = createSignal({});
const [isConnected, setIsConnected] = createSignal(false);
const [discoveryStatus, setDiscoveryStatus] = createSignal('idle'); // 'idle', 'loading', 'success', 'error'

export const blackboard = {
    vfs,
    graph,
    schemas,
    isConnected,
    discoveryStatus,
    async start() {
        console.log(`[UX] Mesh-VFS initialized. Connecting to nearest peer: ${vfsUrl}`);
        
        vfs.events.on('state', async (event) => {
            if (!event.path) return;
            setGraph(prev => ({
                ...prev,
                [event.cid]: { ...prev[event.cid], ...event }
            }));
        });

        // Track connectivity
        try {
            await mesh.fetch(`${vfsUrl}/health`);
            setIsConnected(true);
            console.log('[UX] Successfully connected to mesh at:', vfsUrl);
        } catch (e) {
            setIsConnected(false);
            console.error('[UX] Failed to connect to mesh at:', vfsUrl, e);
        }

        await mesh.start();
        setIsConnected(true);
        console.log('[UX] MeshLink started.');
        
        this.discoverSchemas();
    },
    async discoverSchemas() {
        setDiscoveryStatus('loading');
        try {
            console.log('[UX] Spying on network for sys/schema...');
            const stream = await vfs.spy('sys/schema', {});
            
            if (!stream) {
                console.warn('[UX] Spy returned no stream for sys/schema');
                setDiscoveryStatus('empty');
                return;
            }

            let foundCount = 0;
            for await (const chunk of VFS.parseVFSBundle(stream)) {
                try {
                    const schemaData = JSON.parse(new TextDecoder().decode(chunk.data));
                    const targetPath = chunk.selector.parameters.target;
                    console.log(`[UX] Discovered schema for ${targetPath}:`, schemaData);
                    
                    if (targetPath && schemaData) {
                        foundCount++;
                        setSchemas(prev => ({
                            ...prev,
                            [targetPath]: schemaData
                        }));
                    }
                } catch (e) {
                    console.warn('[UX] Failed to parse schema chunk', e);
                }
            }
            
            setDiscoveryStatus(foundCount > 0 ? 'success' : 'empty');
            console.log(`[UX] Discovery complete. Found ${foundCount} schemas.`);
        } catch (e) {
            console.error('[UX] Error during schema discovery:', e);
            setDiscoveryStatus('error');
        }
    },
    stop() {
        mesh.stop();
        vfs.close();
    },
    async read(path, parameters = {}) {
        return vfs.read(path, parameters);
    },
    async tickle(path, parameters = {}) {
        return vfs.read(path, parameters);
    },
    async write(path, parameters = {}, data) {
        return vfs.writeData(path, parameters, data);
    }
};

if (typeof window !== 'undefined') {
    window.blackboard = blackboard;
}
