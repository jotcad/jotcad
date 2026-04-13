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
const [pulse, setPulse] = createSignal([]); // Array of recent notifications
const [meshTopology, setMeshTopology] = createSignal({ peers: [] });
const [isConnected, setIsConnected] = createSignal(false);
const [discoveryStatus, setDiscoveryStatus] = createSignal('idle'); // 'idle', 'loading', 'success', 'error'

export const blackboard = {
    vfs,
    graph,
    schemas,
    pulse,
    meshTopology,
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

        // Capture Pub-Sub Pulses
        const meshMap = new Map(); // Global Adjacency List: PeerID -> Set of Neighbors
        const originalNotify = mesh.notify.bind(mesh);
        
        mesh.notify = (topic, payload) => {
            originalNotify(topic, payload);
            
            if (payload.type === 'TOPOLOGY_UPDATE') {
                meshMap.set(payload.peer, payload.neighbors);
            }

            setPulse(prev => {
                const next = [...prev, { topic, payload, t: Date.now() }];
                return next.slice(-20); // Keep last 20 pulses
            });
        };

        // ... existing connectivity code ...

        await mesh.start();
        setIsConnected(true);
        console.log('[UX] MeshLink started.');

        // Initial subscription to topology updates
        vfs.read('sys/topo/*', {}, { followLinks: false }).catch(() => {});

        // Topology Sampling Loop
        setInterval(() => {
            const nodes = new Map();
            // 1. Add current node
            nodes.set(vfs.id, { id: vfs.id, type: 'BROWSER', pps: 0, neighbors: [] });

            // 2. Add learned peers from meshMap
            for (const [peerId, neighbors] of meshMap.entries()) {
                if (!nodes.has(peerId)) nodes.set(peerId, { id: peerId, type: 'PEER', pps: 0, neighbors });
                else nodes.get(peerId).neighbors = neighbors;
            }

            // 3. Sync PPS and reachability from direct MeshLink peers
            for (const p of mesh.peers.values()) {
                if (nodes.has(p.id)) {
                    const n = nodes.get(p.id);
                    n.pps = p.pps;
                    n.reachability = p.reachability;
                }
            }

            setMeshTopology({ peers: [...nodes.values()] });
        }, 1000);
        
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
                        vfs.addSchema(targetPath, schemaData);
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
