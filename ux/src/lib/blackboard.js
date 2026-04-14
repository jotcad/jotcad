import { createSignal } from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage } from '../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../jot/src/index.js';

export const vfs = new VFS({ 
    id: 'ui-main',
    storage: new IndexedDBStorage()
});

registerJotProvider(vfs);

const vfsUrl = import.meta.env.VITE_VFS_URL || `http://${window.location.hostname}:9092`;
const mesh = new MeshLink(vfs, [vfsUrl]);

// Reactive State
const [graph, setGraph] = createSignal({});
const [schemas, setSchemas] = createSignal({});
const [pulse, setPulse] = createSignal([]);
const [meshTopology, setMeshTopology] = createStore({ peers: [] });
const [meshPositions, setMeshPositions] = createSignal({}); // PeerID -> {x, y}
const [isConnected, setIsConnected] = createSignal(false);
const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');

export const blackboard = {
    vfs,
    graph,
    schemas,
    pulse,
    meshTopology: () => meshTopology,
    meshPositions,
    setMeshPositions,
    isConnected,
    discoveryStatus,
    async start() {
        console.log(`[UX] Mesh-VFS initialized. Connecting to: ${vfsUrl}`);
        
        // Register Utility Ops (Generators)
        vfs.registerProvider('op/range', async (v, s) => {
            const { start = 0, stop = s.parameters.arg || 0, step = 1 } = s.parameters;
            const res = [];
            for (let i = start; i < stop; i += step) res.push(i);
            return new TextEncoder().encode(JSON.stringify(res));
        }, {
            schema: {
                properties: {
                    start: { type: 'number', default: 0 },
                    stop: { type: 'number' },
                    step: { type: 'number', default: 1 }
                }
            },
            returns: { type: 'array', items: { type: 'number' } }
        });

        vfs.registerProvider('op/iota', async (v, s) => {
            const count = s.parameters.count || s.parameters.arg || 0;
            const res = Array.from({ length: count }, (_, i) => i);
            return new TextEncoder().encode(JSON.stringify(res));
        }, {
            schema: {
                properties: {
                    count: { type: 'number' }
                }
            },
            returns: { type: 'array', items: { type: 'number' } }
        });

        vfs.events.on('state', async (event) => {
            if (!event.path) return;
            setGraph(prev => ({ ...prev, [event.cid]: { ...prev[event.cid], ...event } }));
        });

        const meshMap = new Map();
        const originalNotify = mesh.notify.bind(mesh);
        
        mesh.notify = (selector, payload, stack) => {
            originalNotify(selector, payload, stack);
            if (payload.type === 'TOPOLOGY_UPDATE') {
                meshMap.set(payload.peer, payload.neighbors);
            }
            setPulse(prev => {
                const next = [...prev, { selector, payload, t: Date.now() }];
                return next.slice(-20);
            });
        };

        try {
            await mesh.fetch(`${vfsUrl}/health`);
            setIsConnected(true);
        } catch (e) { setIsConnected(false); }

        await mesh.start();
        setIsConnected(true);

        vfs.read('sys/topo', {}, { followLinks: false }).catch(() => {});

        setInterval(() => {
            const nodes = new Map();
            nodes.set(vfs.id, { id: vfs.id, type: 'BROWSER', pps: 0, neighbors: [] });
            for (const [peerId, neighbors] of meshMap.entries()) {
                if (!nodes.has(peerId)) nodes.set(peerId, { id: peerId, type: 'PEER', pps: 0, neighbors });
                else nodes.get(peerId).neighbors = neighbors;
            }
            for (const p of mesh.peers.values()) {
                if (nodes.has(p.id)) {
                    const n = nodes.get(p.id);
                    n.pps = p.pps;
                    n.reachability = p.reachability;
                }
            }
            setMeshTopology('peers', reconcile([...nodes.values()]));
        }, 1000);
        
        this.discoverSchemas();
    },
    async discoverSchemas() {
        setDiscoveryStatus('loading');
        try {
            const stream = await vfs.spy('sys/schema', {});
            if (!stream) { setDiscoveryStatus('empty'); return; }
            let foundCount = 0;
            for await (const chunk of VFS.parseVFSBundle(stream)) {
                try {
                    const schemaData = JSON.parse(new TextDecoder().decode(chunk.data));
                    const targetPath = chunk.selector.parameters.target;
                    const p = chunk.selector.parameters;
                    const provider = p.provider || p.origin || p.peer || p.source || p.author || 'unknown';
                    schemaData._origin = provider;
                    
                    if (targetPath && schemaData) {
                        console.log(`[Schema Discovery] Registered ${targetPath} with origin: ${provider}`);
                        foundCount++;
                        vfs.addSchema(targetPath, schemaData);
                        setSchemas(prev => {
                            const next = { ...prev, [targetPath]: schemaData };
                            // Debug: show the current catalog state occasionally
                            if (foundCount % 5 === 0) console.table(Object.entries(next).map(([k,v])=>({path:k, origin: v._origin})));
                            return next;
                        });
                    }
                } catch (e) {
                    console.warn('[Schema Discovery] Failed to parse chunk:', e);
                }
            }
            setDiscoveryStatus(foundCount > 0 ? 'success' : 'empty');
        } catch (e) { setDiscoveryStatus('error'); }
    },
    stop() { mesh.stop(); vfs.close(); },
    async read(path, parameters = {}) { return vfs.read(path, parameters); },
    async write(path, parameters = {}, data) { return vfs.writeData(path, parameters, data); }
};

if (typeof window !== 'undefined') { window.blackboard = blackboard; }
