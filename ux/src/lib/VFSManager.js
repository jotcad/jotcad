console.log('[Boot] VFSManager.js loading...');
import { reconcile } from 'solid-js/store';
import { VFS, IndexedDBStorage, Selector } from '../../../fs/src/vfs_browser.js';
export { Selector };
if (typeof window !== 'undefined') {
  window.Selector = Selector;
}
import { MeshLink } from '../../../fs/src/mesh_link.js';
import { registerJotProvider } from '../../../jot/src/index.js';
import { Worksheet } from './vfs/Worksheet';
import { registerUtilityOps } from './vfs/UtilityOps';
import { registerTextureProvider } from './vfs/TextureProvider';
import { JotRegistry } from './vfs/JotRegistry';
import {
  setGraph, setSchemas, setPulse, setMeshTopology,
  setIsConnected, setDiscoveryStatus, setDynamicOps,
  schemas
} from './state/MeshState.js';

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

const getVfsUrl = () => {
    if (typeof window === 'undefined') return import.meta.env.VITE_VFS_URL;
    const params = new URLSearchParams(window.location.search);
    const override = params.get('gateway');
    if (override) {
        // If just a port is provided, assume local https
        if (!override.includes(':')) {
            return `https://localhost:${override}`;
        }
        return override;
    }
    return import.meta.env.VITE_VFS_URL;
};

const vfsUrl = getVfsUrl();
if (!vfsUrl && typeof window !== 'undefined' && !window.__vitest_browser__) {
    // Note: We check for Vitest browser environment to avoid crashing unit tests
    // In a real browser or Puppeteer, this should still throw.
    if (!navigator.userAgent.includes('jsdom')) {
        throw new Error('[VFSManager] CRITICAL: VITE_VFS_URL is not defined and no ?gateway= override provided.');
    }
}

const neighbors = vfsUrl ? [vfsUrl] : [];
export const mesh = new MeshLink(vfs, neighbors);

// --- MESH AUDIT & NOISE CONTROL ---
if (typeof window !== 'undefined') {
    window.__JOT_SUBS = [];
    const originalSubscribe = mesh.subscribe.bind(mesh);
    mesh.subscribe = (selector, expiresAt, stack = []) => {
        const s = Selector.fromObject(selector);
        window.__JOT_SUBS.push({ path: s.path, t: Date.now(), stack });
        console.log(`%c[MeshVFS] -> SUBSCRIBE: ${s.path}`, 'color: #8b5cf6; font-weight: bold;', { selector: s, expiresAt, stack });
        return originalSubscribe(selector, expiresAt, stack);
    };

    const originalNotify = mesh.notify.bind(mesh);
    mesh.notify = (selector, payload, stack = []) => {
        const s = Selector.fromObject(selector);
        if (s.path === 'sys/topo' || s.path === 'sys/schema') {
            return originalNotify(selector, payload, stack);
        }
        const isIncoming = stack.length > 0;
        const color = isIncoming ? '#10b981' : '#3b82f6';
        const label = isIncoming ? '<- NOTIFY' : '-> NOTIFY';
        console.log(`%c[MeshVFS] ${label}: ${selector?.path}`, `color: ${color}; font-weight: bold;`, { selector, payload, stack });
        return originalNotify(selector, payload, stack);
    };
}

let isStarted = false;
const meshMap = new Map();

export const vfsActions = {
  async discoverSchemas(retryCount = 0) {
    setDiscoveryStatus('loading');
    console.log(`[MeshVFS] discoverSchemas attempt ${retryCount + 1}`);
    console.log(`[MeshVFS] -> Subscribing to sys/schema (Catalog Discovery)`);
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
    registerTextureProvider(vfs, mesh);

    // 3. Event Handling
    vfs.events.on('state', async (event) => {
      if (!event.selector) return;
      setGraph((prev) => ({ ...prev, [event.cid]: { ...prev[event.cid], ...event } }));
    });

    // MESH TRACING: Instrument mesh for visibility
    const originalSubscribe = mesh.subscribe.bind(mesh);
    mesh.subscribe = (selector, expiresAt, stack = []) => {
        console.log(`%c[MeshVFS] -> SUBSCRIBE: ${selector.path}`, 'color: #8b5cf6; font-weight: bold;', { selector, expiresAt, stack });
        return originalSubscribe(selector, expiresAt, stack);
    };

    const originalNotify = mesh.notify.bind(mesh);
    mesh.notify = (selector, payload, stack = []) => {
      const s = Selector.fromObject(selector);
      if (s.path === 'sys/topo') {
          const peerId = payload.id || payload.peer;
          console.log(`[MeshVFS] Mesh topology update from ${peerId}: ${payload.neighbors?.length || 0} peers, ${payload.interests?.length || 0} interests.`);
          meshMap.set(peerId, {
            neighbors: payload.neighbors || [],
            interests: payload.interests || []
          });
      }
      if (s.path === 'sys/schema') {
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
      
      const isIncoming = stack.length > 0;
      const color = isIncoming ? '#10b981' : '#3b82f6';
      const label = isIncoming ? '<- NOTIFY' : '-> NOTIFY';
      console.log(`%c[MeshVFS] ${label}: ${selector?.path}`, `color: ${color}; font-weight: bold;`, { selector, payload, stack });
      
      setPulse((prev) => [...prev, { selector, payload, t: Date.now() }].slice(-20));
      return originalNotify(selector, payload, stack);
    };

    try {
      console.log(`[MeshVFS] Initializing mesh connection to: ${vfsUrl}`);
      await mesh.start();
      setIsConnected(true);
      console.log(`[MeshVFS] Mesh started. Discovery active.`);
      
      // Subscribe to topology updates so we receive TOPOLOGY_UPDATE notifications
      mesh.subscribe(new Selector('sys/topo'), Date.now() + 1000 * 60 * 60 * 24).catch((err) => {
        console.error('[MeshVFS] Failed to subscribe to sys/topo:', err);
      });

      // Trigger initial discovery automatically
      this.discoverSchemas();
    } catch (e) {
      console.error('[MeshVFS] Mesh start failed:', e);
      setIsConnected(false);
    }

    setInterval(() => {
      const nodes = new Map();
      const localInterests = [];
      try {
          for (const entry of mesh.interests.values()) {
            if (entry.localExpiresAt > Date.now() && entry.selector && entry.selector.path) {
                localInterests.push({ path: entry.selector.path, local: true });
            }
          }
      } catch (e) { console.error('[MeshVFS] Interest extraction error:', e); }

      nodes.set(vfs.id, { id: vfs.id, type: 'BROWSER', pps: 0, neighbors: [], interests: localInterests });
      
      for (const [peerId, data] of meshMap.entries()) {
        const { neighbors, interests } = data;
        if (!nodes.has(peerId)) {
            nodes.set(peerId, { id: peerId, type: 'PEER', pps: 0, neighbors, interests: interests || [] });
        } else {
            const n = nodes.get(peerId);
            n.neighbors = neighbors;
            n.interests = interests || [];
        }
      }

      // Add missing neighbor nodes to the topology list so they render as leaf nodes!
      for (const data of meshMap.values()) {
        const { neighbors } = data;
        for (const neighbor of neighbors) {
          if (!nodes.has(neighbor.id)) {
            nodes.set(neighbor.id, { id: neighbor.id, type: 'PEER', pps: 0, neighbors: [], interests: [] });
          }
        }
      }

      for (const p of mesh.peers.values()) {
        if (nodes.has(p.id)) {
          const n = nodes.get(p.id);
          n.pps = p.pps;
          n.reachability = p.reachability;
          n.protocol = p.protocol;
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
    return JotRegistry.publishDynamicOp(vfs, mesh, path, schema, script, persist);
  },

  normalizePath(name) {
    return JotRegistry.normalizePath(name);
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

  async _drainStream(result, selectorLabel) {
    if (!result) {
        console.warn(`[MeshVFS] Drain failed: No result for ${selectorLabel}`);
        return null;
    }
    const { stream, metadata } = result;
    
    const reader = stream.getReader();
    const chunks = [];
    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
        }
    } finally {
        reader.releaseLock();
    }

    const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.length;
    }

    const contentType = metadata.contentType || '';
    let parsed;
    if (contentType.includes('application/json')) {
        parsed = JSON.parse(new TextDecoder().decode(bytes));
    } else if (contentType.startsWith('text/')) {
        parsed = new TextDecoder().decode(bytes);
    } else {
        parsed = bytes;
    }
    return parsed;
  },

  async readSelectorData(selector, context = {}) {
    const result = await vfs.readSelector(selector, context);
    return this._drainStream(result, selector.path || 'selector');
  },

  async readCIDData(cid, context = {}) {
    const result = await vfs.readCID(cid, context);
    return this._drainStream(result, cid);
  },

  async writeData(selector, data, metadata = {}) {
    return vfs.write(selector, data, metadata);
  },

  async readSelector(selector, context = {}) {
    return vfs.readSelector(selector, context);
  },

  async readCID(cid, context = {}) {
    return vfs.readCID(cid, context);
  },

  async write(selector, data) {
    return vfs.write(selector, data);
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
