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
  setIsConnected, setDiscoveryStatus, setDynamicOps
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

    console.log(`[MeshVFS] Starting... Peer: ${peerId}`);
    try {
        await vfs.init();
        console.log(`[MeshVFS] VFS Initialized.`);
    } catch (e) {
        console.error(`[MeshVFS] VFS Init Failed:`, e);
    }

    // 1. LOAD OPERATORS
    const ops = Worksheet.get(Worksheet.TIERS.OPERATORS);
    if (ops && typeof ops === 'object') {
        console.log(`[MeshVFS] Found stored ops:`, Object.keys(ops));
        setDynamicOps(ops);
        for (const [path, op] of Object.entries(ops)) {
          try {
            JotRegistry.publishDynamicOp(vfs, mesh, path, op.schema, op.script, false);
          } catch (e) {
            console.error(`[MeshVFS] Failed to publish stored op ${path}:`, e);
          }
        }
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
      console.log(`[MeshVFS] Mesh started. Discovery active.`);
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

  publishDynamicOp(path, schema, script, persist = true) {
    JotRegistry.publishDynamicOp(vfs, mesh, path, schema, script, persist);
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
    console.log(`[MeshVFS] Clearing Storage...`);
    if (vfs.storage && typeof vfs.storage.wipe === 'function') {
        await vfs.storage.wipe();
    }
    localStorage.clear();
    sessionStorage.removeItem('jotcad_peer_id');
    window.location.reload();
  }
};
