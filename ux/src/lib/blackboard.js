import { createSignal } from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage, Selector } from '../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../jot/src/index.js';

export const vfs = new VFS({
  id: 'ui-main',
  storage: new IndexedDBStorage(),
});

registerJotProvider(vfs);

const vfsUrl =
  import.meta.env.VITE_VFS_URL || `http://${window.location.hostname}:9092`;
const mesh = new MeshLink(vfs, [vfsUrl]);

// Reactive State
const [graph, setGraph] = createSignal({});
const [schemas, setSchemas] = createSignal({});
const [pulse, setPulse] = createSignal([]);
const [meshTopology, setMeshTopology] = createStore({ peers: [] });
const [meshPositions, setMeshPositions] = createSignal({}); // PeerID -> {x, y}
const [isConnected, setIsConnected] = createSignal(false);
const [discoveryStatus, setDiscoveryStatus] = createSignal('idle');
const [dynamicOps, setDynamicOps] = createSignal({}); // Path -> { schema, script }

let isStarted = false;
const meshMap = new Map();

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
  dynamicOps,

  async start() {
    if (isStarted) return;
    isStarted = true;

    await vfs.init();

    console.log(`[UX] Mesh-VFS initialized. Connecting to: ${vfsUrl}`);

    // Load Local Dynamic Ops from localStorage
    const saved = localStorage.getItem('jot_dynamic_ops');
    if (saved) {
      try {
        const ops = JSON.parse(saved);
        setDynamicOps(ops);
        console.log(`[UX] Loaded ${Object.keys(ops).length} dynamic operations.`);
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
          properties: {
            start: { type: 'number', default: 0 },
            stop: { type: 'number' },
            step: { type: 'number', default: 1 },
          },
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
          properties: {
            count: { type: 'number' },
          },
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
        console.log(`[UX] Received Catalog Announcement from ${provider}:`, Object.keys(catalog || {}));
        if (catalog) {
          setSchemas((prev) => {
            const next = { ...prev };
            for (const [path, schema] of Object.entries(catalog)) {
              console.log(`  - ${path} (aliases: ${JSON.stringify(schema.aliases || [])})`);
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
      await mesh.fetch(`${vfsUrl}/health`);
      setIsConnected(true);
    } catch (e) {
      setIsConnected(false);
    }

    await mesh.start();
    setIsConnected(true);

    vfs.read(new Selector('sys/topo'), { followLinks: false }).catch(() => {});

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
      if (schema.arguments) {
        for (const [key, arg] of Object.entries(schema.arguments)) {
           if (params[key] === undefined && arg.default !== undefined) {
             params[key] = arg.default;
           }
        }
      }

      const result = await compiler.evaluate(ast, params);
      const primary = Array.isArray(result) ? result[0] : result;
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
        localStorage.setItem('jot_dynamic_ops', JSON.stringify(next));
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
};

if (typeof window !== 'undefined') {
  window.blackboard = blackboard;
}
