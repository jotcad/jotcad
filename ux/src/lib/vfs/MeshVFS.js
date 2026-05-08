import { reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage, Selector } from '../../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../../jot/src/index.js';
import {
  setGraph, setSchemas, setPulse, setMeshTopology,
  setIsConnected, setDiscoveryStatus, setDynamicOps,
  DYNAMIC_OPS_KEY
} from '../state/AppState.js';
import { RemoteStorageHandler } from './RemoteStorageHandler.js';

const getSessionId = () => {
  const prefix = import.meta.env.VITE_STORAGE_PREFIX || 'ui';
  return `${prefix}-${Math.random().toString(36).slice(2, 8)}`;
};

export const peerId = getSessionId();
const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
const VFS_DB_NAME = `${storagePrefix}-vfs`;

export const vfs = new VFS({
  id: peerId,
  storage: new IndexedDBStorage(VFS_DB_NAME),
});

registerJotProvider(vfs);

const vfsUrl =
  import.meta.env.VITE_VFS_URL || `${window.location.protocol}//${window.location.hostname}:9092`;

export const mesh = new MeshLink(vfs, [vfsUrl]);

let isStarted = false;
const meshMap = new Map();

export const vfsActions = {
  async discoverSchemas() {
    setDiscoveryStatus('loading');
    try {
      await mesh.subscribe(new Selector('sys/schema'), Date.now() + 60000);
      setDiscoveryStatus('success');
    } catch (err) {
      console.error('[MeshVFS] Catalog discovery failed:', err);
      setDiscoveryStatus('error');
    }
  },

  async start(blackboard) {
    if (isStarted) return;
    isStarted = true;

    RemoteStorageHandler.init();

    await vfs.init();
    console.log(`[MeshVFS] Initialized. Connecting to: ${vfsUrl}`);

    const saved = localStorage.getItem(DYNAMIC_OPS_KEY);
    if (saved) {
      try {
        const ops = JSON.parse(saved);
        setDynamicOps(ops);
        for (const [path, op] of Object.entries(ops)) {
          blackboard.publishDynamicOp(path, op.schema, op.script, false);
        }
      } catch (e) {
        console.error('[MeshVFS] Failed to load dynamic ops:', e);
      }
    }

    // Register Utility Ops
    vfs.registerProvider('jot/range', async (v, s) => {
        const { start = 0, stop = s.parameters.arg || 0, step = 1 } = s.parameters;
        const res = [];
        for (let i = start; i < stop; i += step) res.push(i);
        return new TextEncoder().encode(JSON.stringify(res));
      }, { schema: { arguments: [{ name: 'start', type: 'number', default: 0 }, { name: 'stop', type: 'number' }, { name: 'step', type: 'number', default: 1 }] } }
    );

    vfs.registerProvider('jot/iota', async (v, s) => {
        const count = s.parameters.count || s.parameters.arg || 0;
        const res = Array.from({ length: count }, (_, i) => i);
        return new TextEncoder().encode(JSON.stringify(res));
      }, { schema: { arguments: [{ name: 'count', type: 'number' }] } }
    );

    vfs.events.on('state', async (event) => {
      if (!event.selector) return;
      setGraph((prev) => ({ ...prev, [event.cid]: { ...prev[event.cid], ...event } }));
    });

    const originalNotify = mesh.notify.bind(mesh);
    mesh.notify = (selector, payload, stack = []) => {
      if (payload.type === 'TOPOLOGY_UPDATE') meshMap.set(payload.peer, payload.neighbors);
      if (payload.type === 'CATALOG_ANNOUNCEMENT') {
        const { catalog, provider } = payload;
        if (catalog) {
          setSchemas((prev) => {
            const next = { ...prev };
            for (const [path, schema] of Object.entries(catalog)) {
              const schemaWithOrigin = { ...schema, _origin: provider };
              vfs.addSchema(path, schemaWithOrigin);
              next[path] = schemaWithOrigin;
            }
            return next;
          });
        }
      }
      setPulse((prev) => [...prev, { selector, payload, t: Date.now() }].slice(-20));
      originalNotify(selector, payload, stack);
    };

    try {
      await mesh.start();
      setIsConnected(true);
    } catch (e) {
      console.error('[MeshVFS] Mesh start failed:', e);
      setIsConnected(false);
    }

    mesh.subscribe(new Selector('sys/schema'));

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
  },

  publishDynamicOp(path, schema, script, persist = true, blackboard) {
    console.log(`[MeshVFS] Publishing Dynamic Op: ${path}`);

    if (schema.arguments && !Array.isArray(schema.arguments)) {
      throw new Error(`Catalog Error: Arguments for operator '${path}' must be an array.`);
    }

    vfs.registerProvider(path, async (v, s) => {
      const { JotCompiler } = await import('../../../../jot/src/compiler');
      const { JotParser } = await import('../../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = blackboard.schemas();
      for (const [p, sch] of Object.entries(currentSchemas)) {
        const name = p.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path: p, schema: sch });
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
      const shapeData = await v.readData(primary);
      return new TextEncoder().encode(JSON.stringify(shapeData));
    }, { schema });

    const schemaWithOrigin = { ...schema, _origin: vfs.id };
    setSchemas(prev => ({ ...prev, [path]: schemaWithOrigin }));
    vfs.addSchema(path, schemaWithOrigin);

    setDynamicOps(prev => {
      const next = { ...prev, [path]: { schema, script } };
      if (persist) {
        localStorage.setItem(DYNAMIC_OPS_KEY, JSON.stringify(next));
        // Sync to RemoteStorage if connected
        RemoteStorageHandler.pushOperator(path, script, schema);
      }
      return next;
    });

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

  async read(selector) {
    return vfs.read(selector);
  },

  async write(selector, data) {
    return vfs.writeData(selector, data);
  },

  async clearStorage() {
    console.log(`[MeshVFS] Clearing Storage...`);
    if (vfs.storage && typeof vfs.storage.wipe === 'function') {
        await vfs.storage.wipe();
    }
    localStorage.removeItem(DYNAMIC_OPS_KEY);
    const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
    localStorage.removeItem(`${storagePrefix}_node_state`);
    sessionStorage.removeItem('jotcad_peer_id');
    window.location.reload();
  }
};
