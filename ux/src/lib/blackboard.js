import { createSignal } from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage, Selector } from '../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../jot/src/index.js';

// Generate a unique ID for this tab/session to prevent mesh conflicts
const getSessionId = () => {
  const key = 'jotcad_peer_id';
  let id = sessionStorage.getItem(key);
  if (!id) {
    id = `ui-${Math.random().toString(36).slice(2, 8)}`;
    sessionStorage.setItem(key, id);
  }
  return id;
};

const peerId = getSessionId();

const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
const DYNAMIC_OPS_KEY = `${storagePrefix}_dynamic_ops`;
const NODE_STATE_KEY = `${storagePrefix}_node_state`;
const VFS_DB_NAME = `${storagePrefix}-vfs`;

export const vfs = new VFS({
  id: peerId,
  storage: new IndexedDBStorage(VFS_DB_NAME),
});

registerJotProvider(vfs);

const vfsUrl =
  import.meta.env.VITE_VFS_URL || `${window.location.protocol}//${window.location.hostname}:9092`;
console.log(`[UX] Target VFS URL: ${vfsUrl} (Protocol: ${window.location.protocol}, Storage: ${storagePrefix})`);
const mesh = new MeshLink(vfs, [vfsUrl]);

// Reactive State
const [graph, setGraph] = createSignal({});
const [schemas, setSchemas] = createSignal({});
const [pulse, setPulse] = createSignal([]);
const [meshTopology, setMeshTopology] = createStore({ peers: [] });
const [meshPositions, setMeshPositions] = createSignal({}); // PeerID -> {x, y}
const [logs, setLogs] = createSignal([]);

// Capture logs for on-screen console
const originalLog = console.log;
const originalWarn = console.warn;
const originalError = console.error;

const addLog = (type, args) => {
  const msg = args.map(a => 
    typeof a === 'object' ? JSON.stringify(a, (key, value) => 
      typeof value === 'bigint' ? value.toString() : value, 2) : String(a)
  ).join(' ');
  
  setLogs(prev => [{ type, msg, t: Date.now(), id: Math.random() }, ...prev].slice(0, 100));
};

console.log = (...args) => { originalLog(...args); addLog('log', args); };
console.warn = (...args) => { originalWarn(...args); addLog('warn', args); };
console.error = (...args) => { originalError(...args); addLog('error', args); };

console.log("[UX] Console redirection active. Peer ID:", peerId, "Storage Prefix:", storagePrefix);

const [isConnected, setIsConnected] = createSignal(false);
const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');
const [dynamicOps, setDynamicOps] = createSignal({}); // Path -> { schema, script }
const [error, setError] = createSignal(null);

let isStarted = false;
const meshMap = new Map();

export const blackboard = {
  vfs,
  peerId,
  graph,
  schemas,
  pulse,
  meshTopology: () => meshTopology,
  meshPositions,
  setMeshPositions,
  logs,
  isConnected,
  discoveryStatus,
  dynamicOps,
  error,
  setError,

  getNodeState() {
    try {
      const saved = localStorage.getItem(NODE_STATE_KEY);
      return saved ? JSON.parse(saved) : {};
    } catch (e) { return {}; }
  },

  saveNodeState(state) {
    localStorage.setItem(NODE_STATE_KEY, JSON.stringify(state));
  },

  async discoverSchemas() {
    setDiscoveryStatus('loading');
    try {
      await mesh.subscribe(new Selector('sys/schema'), Date.now() + 60000);
      setDiscoveryStatus('success');
    } catch (err) {
      console.error('[UX] Catalog discovery failed:', err);
      setDiscoveryStatus('error');
    }
  },

  async start() {
    if (isStarted) return;
    isStarted = true;

    await vfs.init();

    console.log(`[UX] Mesh-VFS initialized. Connecting to: ${vfsUrl}`);

    // Load Local Dynamic Ops from localStorage
    const saved = localStorage.getItem(DYNAMIC_OPS_KEY);
    if (saved) {
      try {
        const ops = JSON.parse(saved);
        setDynamicOps(ops);
        console.log(`[UX] Loaded ${Object.keys(ops).length} dynamic operations from ${DYNAMIC_OPS_KEY}.`);
        // We will publish them after utility ops are registered
        for (const [path, op] of Object.entries(ops)) {
          this.publishDynamicOp(path, op.schema, op.script, false);
        }
      } catch (e) {
        console.error('[UX] Failed to load dynamic ops:', e);
      }
    }

    // Register Utility Ops (Generators)
    vfs.registerProvider(
      'jot/range',
      async (v, s) => {
        const {
          start = 0,
          stop = s.parameters.arg || 0,
          step = 1,
        } = s.parameters;
        const res = [];
        for (let i = start; i < stop; i += step) res.push(i);
        return new TextEncoder().encode(JSON.stringify(res));
      },
      {
        schema: {
          arguments: [
            { name: 'start', type: 'number', default: 0 },
            { name: 'stop', type: 'number' },
            { name: 'step', type: 'number', default: 1 },
          ],
        },
        returns: { type: 'array', items: { type: 'number' } },
      }
    );

    vfs.registerProvider(
      'jot/iota',
      async (v, s) => {
        const count = s.parameters.count || s.parameters.arg || 0;
        const res = Array.from({ length: count }, (_, i) => i);
        return new TextEncoder().encode(JSON.stringify(res));
      },
      {
        schema: {
          arguments: [
            { name: 'count', type: 'number' },
          ],
        },
        returns: { type: 'array', items: { type: 'number' } },
      }
    );

    vfs.events.on('state', async (event) => {
      if (!event.selector) return;
      setGraph((prev) => ({
        ...prev,
        [event.cid]: { ...prev[event.cid], ...event },
      }));
    });

    const originalNotify = mesh.notify.bind(mesh);
    mesh.notify = (selector, payload, stack = []) => {
      // 1. Local Processing (Before Loop Prevention)
      if (payload.type === 'TOPOLOGY_UPDATE') {
        meshMap.set(payload.peer, payload.neighbors);
      }

      if (payload.type === 'CATALOG_ANNOUNCEMENT') {
        const { catalog, provider } = payload;
        console.log(`[UX] Received Catalog Announcement from ${provider}:`, Object.keys(catalog || {}).join(', '));
        if (catalog) {
          setSchemas((prev) => {
            const next = { ...prev };
            for (const [path, schema] of Object.entries(catalog)) {
              console.log(`[UX]   - Updating Schema: ${path}`);
              const schemaWithOrigin = { ...schema, _origin: provider };
              vfs.addSchema(path, schemaWithOrigin);
              next[path] = schemaWithOrigin;
            }
            return next;
          });
        }
      }

      setPulse((prev) => {
        const next = [...prev, { selector, payload, t: Date.now() }];
        return next.slice(-20);
      });

      // 2. Mesh Forwarding (With Loop Prevention)
      originalNotify(selector, payload, stack);
    };

    try {
      await mesh.start();
      setIsConnected(true);
    } catch (e) {
      console.error('[UX] Mesh start failed:', e);
      blackboard.setError(e);
      setIsConnected(false);
    }

    // Subscribe to Schema Announcements from the mesh
    mesh.subscribe(new Selector('sys/schema'));

    setInterval(() => {
      const nodes = new Map();
      nodes.set(vfs.id, { id: vfs.id, type: 'BROWSER', pps: 0, neighbors: [] });
      for (const [peerId, neighbors] of meshMap.entries()) {
        if (!nodes.has(peerId))
          nodes.set(peerId, { id: peerId, type: 'PEER', pps: 0, neighbors });
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
  },

  publishDynamicOp(path, schema, script, persist = true) {
    console.log(`[UX] Publishing Dynamic Op: ${path}`);

    if (schema.arguments && !Array.isArray(schema.arguments)) {
      throw new Error(`Catalog Error: Arguments for operator '${path}' must be an array to preserve positional order.`);
    }

    // 1. Register VFS Provider
    vfs.registerProvider(path, async (v, s) => {
      const { JotCompiler } = await import('../../../jot/src/compiler');
      const { JotParser } = await import('../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = this.schemas();
      for (const [p, sch] of Object.entries(currentSchemas)) {
        compiler.registerOperator(p.startsWith('jot/') ? p.slice(4) : p, { path: p, schema: sch });
      }

      const parser = new JotParser();
      const ast = parser.parse(script);
      
      const params = { ...s.parameters };
      if (schema.arguments && Array.isArray(schema.arguments)) {
        for (const arg of schema.arguments) {
           if (params[arg.name] === undefined && arg.default !== undefined) {
             params[arg.name] = arg.default;
           }
        }
      }

      const result = await compiler.evaluate(ast, params);
      const primary = Array.isArray(result) ? result[0] : result;
      // CRITICAL: Must pass the formal Selector instance to readData to preserve its output port
      const shapeData = await v.readData(primary);
      return new TextEncoder().encode(JSON.stringify(shapeData));
    }, { schema });

    // 2. Update reactive schemas
    const schemaWithOrigin = { ...schema, _origin: vfs.id };
    setSchemas(prev => ({ ...prev, [path]: schemaWithOrigin }));
    vfs.addSchema(path, schemaWithOrigin);

    // 3. Update dynamicOps state
    setDynamicOps(prev => {
      const next = { ...prev, [path]: { schema, script } };
      if (persist) {
        localStorage.setItem(DYNAMIC_OPS_KEY, JSON.stringify(next));
      }
      return next;
    });

    // 4. Announce to mesh
    mesh.notify(new Selector('sys/schema'), {
      type: 'CATALOG_ANNOUNCEMENT',
      provider: vfs.id,
      catalog: { [path]: schemaWithOrigin }
    });
  },

  stop() {
    mesh.stop();
    vfs.close();
  },

  async read(path, parameters = {}) {
    return vfs.read(new Selector(path, parameters));
  },

  async write(path, parameters = {}, data) {
    return vfs.writeData(new Selector(path, parameters), data);
  },

  async clearStorage() {
    console.log(`[UX] Clearing Storage (${storagePrefix})...`);
    // 1. Wipe VFS storage (IndexedDB)
    if (vfs.storage && typeof vfs.storage.wipe === 'function') {
        await vfs.storage.wipe();
    }
    // 2. Clear dynamic ops and UI state from localStorage
    localStorage.removeItem(DYNAMIC_OPS_KEY);
    localStorage.removeItem(NODE_STATE_KEY);
    // 3. Clear session ID to force a fresh identity
    sessionStorage.removeItem('jotcad_peer_id');
    
    console.log('[UX] Storage cleared. Reloading page...');
    window.location.reload();
  }
};

if (typeof window !== 'undefined') {
  window.blackboard = blackboard;
}
