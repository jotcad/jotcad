console.log('[Boot] VFSManager.js loading...');
import { reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage, Selector } from '../../../../fs/src/vfs_browser.js';
import { MeshLink } from '../../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../../jot/src/index.js';
import { Worksheet } from './Worksheet';
import { registerUtilityOps } from './UtilityOps';
import { JotRegistry } from './JotRegistry';
import {
  setGraph, setSchemas, setPulse, setMeshTopology,
  setIsConnected, setDiscoveryStatus, setDynamicOps,
  schemas
} from '../state/MeshState.js';

console.log('[Boot] VFSManager.js imports resolved.');

const getSessionId = () => {
  return `ui-${Math.random().toString(36).slice(2, 8)}`;
};

export const peerId = getSessionId();
const VFS_DB_NAME = `jot-vfs`;

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
  async discoverSchemas(retryCount = 0) {
    setDiscoveryStatus('loading');
    console.log(`[MeshVFS] discoverSchemas attempt ${retryCount + 1}`);
    try {
      await mesh.subscribe(new Selector('sys/schema'), Date.now() + 60000);
      setDiscoveryStatus('success');

      // If we have no peers yet, the sub was likely dropped. Retry in 1s, then 2s, then 4s...
      if (mesh.peers.size === 0 && retryCount < 5) {
          console.log('[MeshVFS] No peers yet, scheduling discovery retry...');
          setTimeout(() => this.discoverSchemas(retryCount + 1), Math.pow(2, retryCount) * 1000);
      }
    } catch (err) {
      console.error('[MeshVFS] Catalog discovery failed:', err);
      setDiscoveryStatus('error');
    }
  },

  async start(blackboard) {
    if (isStarted) return;
    isStarted = true;

    console.log(`[MeshVFS] Starting... Peer: ${peerId}`);
    try {
        await vfs.init();
        console.log(`[MeshVFS] VFS Initialized.`);
    } catch (e) {
        console.error(`[MeshVFS] VFS Init Failed:`, e);
    }

    // 1. LOAD OPERATORS (Latest Version Purge)
    const ops = Worksheet.get(Worksheet.TIERS.OPERATORS);
    if (ops && typeof ops === 'object') {
        console.log(`[MeshVFS] Found stored ops:`, Object.keys(ops));
        
        // Group by base name to find latest versions
        const groups = new Map(); // baseName -> { path, version, op }
        for (const [path, op] of Object.entries(ops)) {
            const [base, vStr] = path.split(':v');
            const version = vStr ? parseInt(vStr) : 0;
            if (!groups.has(base) || groups.get(base).version < version) {
                groups.set(base, { path, version, op });
            }
        }

        const cleanedOps = {};
        let changed = false;

        // Only attempt to publish and keep the LATEST for each name
        for (const { path, op } of groups.values()) {
          try {
            JotRegistry.publishDynamicOp(vfs, mesh, path, op.schema, op.script, false);
            cleanedOps[path] = op;
          } catch (e) {
            console.error(`[MeshVFS] Defective operator detected and purged: ${path}`, e);
            changed = true;
          }
        }

        // If we have fewer ops than we started with, we purged rubbish
        if (Object.keys(cleanedOps).length < Object.keys(ops).length || changed) {
            console.log(`[MeshVFS] Rubbish purged. Reduced library from ${Object.keys(ops).length} to ${Object.keys(cleanedOps).length} entries.`);
            Worksheet.save(Worksheet.TIERS.OPERATORS, null, cleanedOps, false);
        }
        
        setDynamicOps(cleanedOps);
    }

    // 2. Register Utility Ops
    registerUtilityOps(vfs);

    // 3. Event Handling
    vfs.events.on('state', async (event) => {
      if (!event.selector) return;
      setGraph((prev) => ({ ...prev, [event.cid]: { ...prev[event.cid], ...event } }));
    });

    const originalNotify = mesh.notify.bind(mesh);
    mesh.notify = (selector, payload, stack = []) => {
      if (payload.type === 'TOPOLOGY_UPDATE') meshMap.set(payload.peer, payload.neighbors);
      if (payload.type === 'CATALOG_ANNOUNCEMENT') {
        const { catalog, provider } = payload;
        console.log(`%c[MeshVFS] Received Catalog from ${provider} (${Object.keys(catalog || {}).length} ops)`, 'color: #f59e0b; font-weight: bold;');
        
        if (catalog) {
          setSchemas((prev) => {
            const next = { ...prev };
            for (const [path, schema] of Object.entries(catalog)) {
              if (path.startsWith('user/') && (!schema.arguments || !Array.isArray(schema.arguments))) {
                console.warn(`[MeshVFS] Skipping malformed user operator '${path}' from ${provider}`);
                continue;
              }
              const schemaWithOrigin = { ...schema, _origin: provider, path };
              vfs.addSchema(path, schemaWithOrigin);
              next[path] = schemaWithOrigin;
              console.log(`[MeshVFS]   Registered: ${path}`);
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
      console.log(`[MeshVFS] Mesh started. Discovery active.`);
      
      // Trigger initial discovery automatically
      this.discoverSchemas();
    } catch (e) {
      console.error('[MeshVFS] Mesh start failed:', e);
      setIsConnected(false);
    }

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

    // Helper for debugging
    window.dumpSchemas = () => {
        console.table(Object.entries(schemas()).map(([path, s]) => ({
            path,
            origin: s._origin,
            args: s.arguments?.length || 0,
            outputs: Object.keys(s.outputs || {}).join(', ')
        })));
    };
  },

  publishDynamicOp(path, schema, script, persist = true) {
    JotRegistry.publishDynamicOp(vfs, mesh, path, schema, script, persist);
  },

  getNextVersionPath(name) {
    return JotRegistry.getNextVersionPath(name);
  },

  removeDynamicOp(path) {
    JotRegistry.removeDynamicOp(vfs, mesh, path);
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
    console.warn(`[MeshVFS] clearStorage CALLED from:`, new Error().stack);
    if (vfs.storage && typeof vfs.storage.wipe === 'function') {
        await vfs.storage.wipe();
    }
    localStorage.clear();
    sessionStorage.removeItem('jotcad_peer_id');
    window.location.reload();
  }
};
