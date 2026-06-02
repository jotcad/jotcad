import { normalizeSelector, Selector, decodeInfo } from '../cid.js';
import { log, info } from '../log.js';
import { ForwardConnection } from './forward_connection.js';
import { ReverseConnection } from './reverse_connection.js';
import { PEER_POLL_TIMEOUT_MS } from './constants.js';
import {
  ReadSelectorRequest,
  ReadCIDRequest,
  SpyRequest,
  SubscribeRequest,
  NotifyRequest
} from './connection.js';

/**
 * MeshLink: Direct Neighbor Registry and Recursive Bread-crumb Router.
 * Honors Identity Duality: Explicit methods for Selector and CID searches.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.vfs.mesh = this;
    this.instanceId = Math.random().toString(36).slice(2, 8);
    this.fetch = options.fetch || globalThis.fetch.bind(globalThis);
    this.localUrl = options.localUrl;
    log(`[MeshLink ${this.vfs.id}] Instance ${this.instanceId} created.`);
    this.peers = new Map(); // neighborId -> Connection
    this.connecting = new Set();
    this.pollers = []; // Private client-side long-polling receivers
    this.neighborUrls = neighborUrls.map((u) => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.interests = []; // List of { selector: Selector, subs: Map<neighborId, expiresAt>, localExpiresAt: number }

    this._ppsInterval = setInterval(() => {
      for (const conn of this.peers.values()) conn._tickPPS();
    }, 1000);
    if (this._ppsInterval.unref) this._ppsInterval.unref();
  }

  /**
   * INTERNAL: Enforce strictly one Connection per peer.
   */
  _setPeer(id, conn) {
    if (this.peers.has(id)) {
      const existing = this.peers.get(id);
      throw new Error(`CRITICAL PROTOCOL VIOLATION: Peer ${id} already has an active connection (${existing.reachability}). Refusing to create duplicate.`);
    }
    this.peers.set(id, conn);
  }

  _getTopologyPayload() {
    this.pruneStaleConnections();
    const activeInterests = [];
    for (const entry of this.interests) {
        const isLocal = entry.localExpiresAt > Date.now();
        const hasRemote = [...entry.subs.values()].some(expiry => expiry > Date.now());
        if ((isLocal || hasRemote) && entry.selector && entry.selector.path) {
            activeInterests.push({
                path: entry.selector.path,
                local: isLocal,
                remote: hasRemote
            });
        }
    }

    const neighbors = [...this.peers.values()].map(c => ({
        id: c.neighborId,
        reachability: c.reachability || 'DIRECT',
        protocol: c.getProtocol()
    }));

    return {
        id: this.vfs.id,
        interests: activeInterests,
        neighbors: neighbors
    };
  }

  pruneStaleConnections() {
    const now = Date.now();
    for (const [id, conn] of this.peers.entries()) {
      if (conn instanceof ReverseConnection) {
        if (now - conn.lastPollAt > PEER_POLL_TIMEOUT_MS) {
          log(`[MeshLink ${this.vfs.id}] Authoritative pruning of stale peer ${id} (no poll for ${now - conn.lastPollAt}ms).`);
          if (conn.stop) conn.stop();
          this.peers.delete(id);
          
          // Scrub interests
          for (const entry of this.interests) {
            entry.subs.delete(id);
          }
        }
      }
    }
  }

  upgradePeerToWS(id, wsConn) {
    let reachability = wsConn.reachability;
    if (this.peers.has(id)) {
      const old = this.peers.get(id);
      if (old.getProtocol && old.getProtocol().includes('ws')) {
        log(`[MeshLink ${this.vfs.id}] Peer ${id} already has active WebSocket connection. Closing redundant new connection.`);
        if (wsConn.stop) wsConn.stop();
        return;
      }
      reachability = old.reachability;
      log(`[MeshLink ${this.vfs.id}] Upgrading peer ${id} to WS. Closing old ${old.reachability} connection.`);
      if (old.stop) old.stop();
    }
    wsConn.reachability = reachability;
    this.peers.set(id, wsConn);
    this.vfs.notify(new Selector('sys/topo'), this._getTopologyPayload());
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    
    // Paint the local interest field
    let entry = this.interests.find(e => e.selector.equals(s));
    if (!entry) {
        entry = { selector: s, subs: new Map(), localExpiresAt: 0 };
        this.interests.push(entry);
    }

    const wasLocal = entry.localExpiresAt > Date.now();
    entry.localExpiresAt = Math.max(entry.localExpiresAt, expiresAt);
    log(`[MeshLink ${this.vfs.id}] Local interest in ${s.path} recorded (expiresAt: ${expiresAt})`);
    
    if (!wasLocal) {
        this.vfs.notify(new Selector('sys/topo'), this._getTopologyPayload());
    }

    // Propagate the interest (Painting the mesh)
    const nextStack = [...stack, this.vfs.id];
    for (const conn of this.peers.values()) {
      if (!nextStack.includes(conn.neighborId)) {
        log(`[MeshLink ${this.vfs.id}] -> Propagating local interest in ${s.path} to peer ${conn.neighborId}`);
        conn.send(new SubscribeRequest(s, expiresAt, nextStack)).catch((err) => {
          log(`[MeshLink ${this.vfs.id}] Failed to propagate subscription interest to peer ${conn.neighborId}: ${err.message}`);
        });
      }
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const s = normalizeSelector(selector);
    log(`[MeshLink ${this.vfs.id}] addInterest: ${s.path} from ${neighborId} (stack: ${stack})`);
    
    let entry = this.interests.find(e => e.selector.equals(s));
    if (!entry) {
      entry = { selector: s, subs: new Map(), localExpiresAt: 0 };
      this.interests.push(entry);
    }
    
    const isNewNeighbor = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);

    if (isNewNeighbor) {
        this.notify(new Selector('sys/topo'), this._getTopologyPayload());
    }

    // Contribution phase (sys/schema, sys/topo)
    if (isNewNeighbor && s.path === 'sys/schema') {
        const conn = this.peers.get(neighborId);
        if (conn) {
            const catalog = this.vfs.getCatalog();
            log(`[MeshLink ${this.vfs.id}] -> Contributing local catalog to ${neighborId} (${Object.keys(catalog.catalog).length} ops)`);
            conn.send(new NotifyRequest(s, catalog, [this.vfs.id])).catch((err) => {
              log(`[MeshLink ${this.vfs.id}] Failed to push local catalog to new peer ${neighborId}: ${err.message}`);
            });
        }
    }

    if (isNewNeighbor && s.path === 'sys/topo') {
        const conn = this.peers.get(neighborId);
        if (conn) {
            log(`[MeshLink ${this.vfs.id}] -> Contributing local topology to new subscriber ${neighborId}`);
            conn.send(new NotifyRequest(s, this._getTopologyPayload(), [this.vfs.id])).catch((err) => {
              log(`[MeshLink ${this.vfs.id}] Failed to push topology to new peer ${neighborId}: ${err.message}`);
            });
        }
    }

    // PROPAGATION PHASE
    if (isNewNeighbor) {
      const nextStack = [...stack, this.vfs.id];
      for (const conn of this.peers.values()) {
        if (!nextStack.includes(conn.neighborId) && conn.neighborId !== neighborId) {
          log(`[MeshLink ${this.vfs.id}] -> Propagating interest in ${s.path} from ${neighborId} to ${conn.neighborId}`);
          conn.send(new SubscribeRequest(s, expiresAt, nextStack)).catch((err) => {
            log(`[MeshLink ${this.vfs.id}] Failed to propagate subscription interest to neighbor ${conn.neighborId}: ${err.message}`);
          });
        }
      }
    }
  }

  notify(selector, payload, stack = []) {
    this.pruneStaleConnections();
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    const nextStack = [...stack, this.vfs.id];

    info(`[MeshLink ${this.vfs.id}] notify: ${s.path} from stack ${stack}`);

    // Logical Entry Matching
    let entry = this.interests.find(e => e.selector.equals(s));
    if (!entry) {
        entry = { selector: s, subs: new Map(), localExpiresAt: 0 };
        this.interests.push(entry);
    }

    // 1. Local Delivery
    if (entry.localExpiresAt > Date.now()) {
      info(`[MeshLink ${this.vfs.id}] -> Local delivery for ${s.path}`);
      this.vfs.events.emit('notify', s, payload);
    }

    // 2. Neighbor Propagation
    for (const [neighborId, expiry] of entry.subs.entries()) {
      if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
      if (stack.includes(neighborId)) continue;
      
      const conn = this.peers.get(neighborId);
      if (conn) {
          info(`[MeshLink ${this.vfs.id}] -> Forwarding notification for ${s.path} to ${neighborId}`);
          conn.send(new NotifyRequest(s, payload, nextStack)).catch((err) => {
            error(`[MeshLink ${this.vfs.id}] Failed to forward notification to neighbor ${neighborId}: ${err.message}`);
          });
      }
    }
  }

  async start() {
    this.vfs.mesh = this;
    await Promise.all(this.neighborUrls.map((url) => this.addPeer(url)));
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');

    // Prevent redundant reciprocal handshakes if we already have an active peer connection pointing to this URL
    for (const conn of this.peers.values()) {
      const connUrl = conn.url || conn.baseUrl;
      if (connUrl === url) {
        log(`[MeshLink ${this.vfs.id}] addPeer: Already connected to URL ${url}, skipping handshake.`);
        return null;
      }
    }

    log(`[MeshLink ${this.vfs.id}] addPeer startup: Attempting connection to ${url}`);
    if (this.connecting.has(url)) {
        log(`[MeshLink ${this.vfs.id}] addPeer: Already connecting to ${url}, skipping.`);
        return null;
    }
    this.connecting.add(url);
    try {
      log(`[MeshLink ${this.vfs.id}] -> POST ${url}/register (vfsId: ${this.vfs.id})`);
      const resp = await this.fetch(`${url}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: this.vfs.id, url: this.localUrl, transports: ['http', 'ws'] }),
        signal: this.abortController.signal || AbortSignal.timeout(5000),
      });
      log(`[MeshLink ${this.vfs.id}] <- ${resp.status} ${url}/register`);
      if (resp.ok) {
        const info = await resp.json();
        log(`[MeshLink ${this.vfs.id}] Registration successful for ${url}:`, info);
        if (info.id && info.id !== this.vfs.id) {
          const existing = this.peers.get(info.id);
          if (existing && (existing.ws || existing.constructor.name.includes('WS'))) {
            log(`[MeshLink ${this.vfs.id}] Peer ${info.id} already has active WebSocket connection. Skipping redundant upgrade.`);
            return info.id;
          }

          // WS upgrade phase - TIE-BREAKER: Only smaller ID initiates
          if (info.transports && info.transports.includes('ws') && info.wsUrl && this.vfs.id < info.id) {
            try {
              log(`[MeshLink ${this.vfs.id}] Initiating deterministic WS upgrade to ${info.id} (Tie-breaker: ${this.vfs.id} < ${info.id})`);
              let wsConn;
              if (typeof window === 'undefined') {
                const { WSNodeForwardConnection } = await import('../vfs_ws_transport_node.js');
                wsConn = await WSNodeForwardConnection.connect(info.wsUrl, this.vfs.id, info.id);
              } else {
                const { WSBrowserForwardConnection } = await import('../vfs_ws_transport_browser.js');
                wsConn = await WSBrowserForwardConnection.connect(info.wsUrl, this.vfs.id, info.id);
              }

              wsConn.vfs = this.vfs;
              wsConn.mesh = this;
              this.upgradePeerToWS(info.id, wsConn);
              log(`[MeshLink ${this.vfs.id}] Successfully upgraded peer ${info.id} to WebSocket!`);
              return info.id;
            } catch (wsErr) {
              log(`[MeshLink ${this.vfs.id}] WebSocket upgrade to ${info.wsUrl} failed: ${wsErr.message}. Falling back to HTTP.`);
            }
          }

          let newConn = null;

          if (!this.peers.has(info.id)) {
            newConn = new ForwardConnection(info.id, url, this.fetch, { 
                localUrl: this.localUrl, 
                signal: this.abortController.signal,
                reachability: info.reachability
            });
            this._setPeer(info.id, newConn);
            log(`[MeshLink ${this.vfs.id}] Peer added: ${info.id} (${info.reachability})`);
          }

          // Reverse poll line for hidden nodes
          if (info.reachability === 'REVERSE' || !this.localUrl) {
            const pollerId = `${info.id}-poller`;
            const hasExistingPoller = this.pollers.some(p => p.neighborId === pollerId);
            if (!hasExistingPoller) {
                log(`[MeshLink ${this.vfs.id}] Starting private reverse poll line for ${info.id}`);
                const poller = new ReverseConnection(pollerId, this, { instanceId: this.instanceId });
                poller.vfs = this.vfs; // Explicit assignment for the polling receiver
                this.pollers.push(poller);
                poller.startPolling(url, this.fetch);
            }
          }

          // SYNC INTERESTS
          if (newConn) {
              for (const entry of this.interests.values()) {
                let maxExp = entry.localExpiresAt || 0;
                for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
                if (maxExp > Date.now()) {
                  log(`[MeshLink ${this.vfs.id}] -> Syncing interest in ${entry.selector.path} to new peer ${newConn.neighborId}`);
                  newConn.send(new SubscribeRequest(entry.selector, maxExp, [this.vfs.id])).catch((err) => {
                    log(`[MeshLink ${this.vfs.id}] Failed to sync interest to new peer ${newConn.neighborId}: ${err.message}`);
                  });
                }
              }
          }

          this.notify(new Selector('sys/topo'), this._getTopologyPayload());
          return info.id;
        }
      } else {
        const errText = await resp.text().catch(() => 'no body');
        log(`[MeshLink ${this.vfs.id}] Handshake rejected by ${url} (Status ${resp.status}): ${errText}`);
      }
    } catch (e) {
        log(`[MeshLink ${this.vfs.id}] Handshake failed for ${url}: ${e.message}`);
    } finally { this.connecting.delete(url); }
    return null;
  }

  async probeDirectReachability(url) {
    if (!url) return false;
    const target = `${url.replace(/\/$/, '')}/health`;
    try {
      const resp = await this.fetch(target, {
        signal: AbortSignal.timeout(1000),
      });
      return resp.ok;
    } catch (e) {
      log(`[MeshLink ${this.vfs.id}] probeDirectReachability failed for ${target}: ${e.message}`);
      return false;
    }
  }

  async registerPeer(peerId, url) {
    if (!peerId) throw new Error('Missing peerId');
    const wsUrl = this.localUrl ? this.localUrl.replace(/^http/, 'ws') + '/vfs-ws' : null;

    if (this.peers.has(peerId)) {
      return { id: this.vfs.id, reachability: this.peers.get(peerId).reachability, transports: ['http', 'ws'], wsUrl };
    }

    if (url) {
      const isReachable = await this.probeDirectReachability(url);
      if (this.peers.has(peerId)) {
        return { id: this.vfs.id, reachability: this.peers.get(peerId).reachability, transports: ['http', 'ws'], wsUrl };
      }
      if (isReachable) {
        const conn = new ForwardConnection(peerId, url, this.fetch, {
          localUrl: this.localUrl,
          signal: this.abortController.signal,
          reachability: 'DIRECT'
        });
        this._setPeer(peerId, conn);
        this.notify(new Selector('sys/topo'), this._getTopologyPayload());
        return { id: this.vfs.id, reachability: 'DIRECT', transports: ['http', 'ws'], wsUrl };
      }
    }

    if (this.peers.has(peerId)) {
      return { id: this.vfs.id, reachability: this.peers.get(peerId).reachability, transports: ['http', 'ws'], wsUrl };
    }
    const conn = new ReverseConnection(peerId, this, { instanceId: this.instanceId });
    this._setPeer(peerId, conn);
    this.notify(new Selector('sys/topo'), this._getTopologyPayload());
    return { id: this.vfs.id, reachability: 'REVERSE', transports: ['http', 'ws'], wsUrl };
  }

  registerReversePeer(neighborId, res, replyTo = null, stream = null, info = null) {
    if (!this.peers.has(neighborId)) {
      this.registerPeer(neighborId);
    }
    const conn = this.peers.get(neighborId);
    if (!(conn instanceof ReverseConnection)) {
        throw new Error(`CRITICAL PROTOCOL VIOLATION: registerReversePeer called for ${neighborId} but connection is ${conn.constructor.name}`);
    }
    conn.addScanner(res, replyTo, stream, info);
  }

  async readSelector(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const targetConns = [...this.peers.values()];
    if (targetConns.length === 0) return null;

    const fetchPromises = targetConns.map(async (conn) => {
      const resp = await conn.send(new ReadSelectorRequest(selector, { ...context, stack, expiresAt }));
      const metadata = decodeInfo(resp?.headers?.get('x-vfs-info'));
      
      if (metadata.error) {
          if (metadata.error.includes('Backflow') || metadata.error.includes('403')) {
              throw new Error(`Backflow: ${metadata.error}`);
          }
          throw new Error(`Peer ${conn.neighborId} reported error: ${metadata.error}`);
      }
      if (resp && resp.body) return { stream: resp.body, metadata };
      throw new Error(`Peer ${conn.neighborId} returned no data for ${selector.path}`);
    });

    try { return await Promise.any(fetchPromises); } 
    catch (e) { 
        const errDetails = e.errors ? e.errors.map(err => err.message).join(', ') : e.message;
        throw new Error(`Mesh-wide Selector Read Failure for ${selector.path}: ${errDetails}`); 
    }
    finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  async readCID(cid, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const targetConns = [...this.peers.values()];
    if (targetConns.length === 0) return null;

    const fetchPromises = targetConns.map(async (conn) => {
      const resp = await conn.send(new ReadCIDRequest(cid, { ...context, stack, resolutionStack, expiresAt }));
      const metadata = decodeInfo(resp?.headers?.get('x-vfs-info'));
      
      if (metadata.error) {
          if (metadata.error.includes('Backflow') || metadata.error.includes('403')) {
              throw new Error(`Backflow: ${metadata.error}`);
          }
          throw new Error(`Peer ${conn.neighborId} reported error: ${metadata.error}`);
      }
      if (resp && resp.body) return { stream: resp.body, metadata };
      throw new Error(`Peer ${conn.neighborId} returned no data for ${cid}`);
    });

    try { return await Promise.any(fetchPromises); } 
    catch (e) { 
        const errDetails = e.errors ? e.errors.map(err => err.message).join(', ') : e.message;
        throw new Error(`Mesh-wide CID Read Failure for ${cid}: ${errDetails}`); 
    }
    finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const nextStack = stack.includes(this.vfs.id) ? stack : [...stack, this.vfs.id];
    const targetConns = [...this.peers.values()].filter(c => !nextStack.includes(c.neighborId));
    
    if (targetConns.length === 0) return null;
    
    const fetchPromises = targetConns.map(async (conn) => {
      try {
        const result = await conn.send(new SpyRequest(s, { ...context, stack: nextStack, expiresAt }));
        return result?.stream || null;
      } catch (err) { return null; }
    });
    
    try {
      const results = await Promise.allSettled(fetchPromises);
      const validStreams = results.filter(r => r.status === 'fulfilled' && r.value).map(r => r.value);
      
      if (validStreams.length === 0) return null;
      return new ReadableStream({
        async start(controller) {
          for (const s of validStreams) {
            const reader = s.getReader();
            try {
              while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                controller.enqueue(value);
              }
            } finally {
              reader.releaseLock();
            }
          }
          controller.close();
        },
      });
    } finally {
      for (const p of fetchPromises) p.catch(() => {});
    }
  }

  stop() { 
    this.abortController.abort(); 
    clearInterval(this._ppsInterval); 
    for (const conn of this.peers.values()) if (conn.stop) conn.stop();
    for (const poller of this.pollers) if (poller.stop) poller.stop();
    this.peers.clear(); 
    this.pollers = [];
  }
}

export class MeshLink extends MeshLinkBase {}
