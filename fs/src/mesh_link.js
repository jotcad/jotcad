import crypto from 'crypto';
import { normalizeSelector, Selector, decodeSafe } from './vfs_core.js';

/**
 * Connection: Abstract interface for a direct communication pipe to a neighbor.
 */
class Connection {
  constructor(neighborId) {
    this.neighborId = neighborId;
    this.reachability = 'UNKNOWN';
    this.lastPulse = 0;
    this.pps = 0;
    this._pulseCount = 0;
  }

  _tickPPS() {
    this.pps = this._pulseCount;
    this._pulseCount = 0;
  }

  async read(selector, context) { throw new Error('Not implemented'); }
  async spy(selector, context) { throw new Error('Not implemented'); }
  async subscribe(selector, expiresAt, stack) { throw new Error('Not implemented'); }
  async notify(selector, payload, stack) { throw new Error('Not implemented'); }
}

/**
 * ForwardConnection: A pipe using outgoing HTTP requests to a stable neighbor URL.
 */
class ForwardConnection extends Connection {
  constructor(neighborId, url, fetch, options = {}) {
    super(neighborId);
    this.url = url.replace(/\/$/, '');
    this.fetch = fetch;
    this.localUrl = options.localUrl;
    this.signal = options.signal;
    this.reachability = options.reachability || 'DIRECT';
  }

  async read(target, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 'Content-Type': 'application/json', 'x-vfs-id': stack[0] };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const bodyObj = { stack, resolutionStack, expiresAt };
    if (typeof target === 'string') {
        bodyObj.cid = target;
    } else {
        bodyObj.selector = target;
    }

    const body = JSON.stringify(bodyObj);
    try {
        console.log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/read`, body.length > 500 ? body.slice(0, 500) + '...' : body);
        const resp = await this.fetch(`${this.url}/read`, {
          method: 'POST',
          headers,
          signal: this.signal || AbortSignal.timeout(5000),
          body,
        });
        console.log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${this.url}/read`);

        if (resp.ok && resp.body) return { body: resp.body, headers: resp.headers };
        
        const errText = await resp.text().catch(() => 'no body');
        throw new Error(`POST /read failed with status ${resp.status}: ${errText}`);
    } catch (err) {
        console.log(`[MeshLink ${this.neighborId}] read error: ${err.message}`);
        throw err;
    }
  }

  async spy(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 'Content-Type': 'application/json', 'x-vfs-id': stack[0] };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const resp = await this.fetch(`${this.url}/spy`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
      body: JSON.stringify({ selector, stack, resolutionStack, expiresAt }),
    });

    if (resp.ok && resp.body) return resp.body;
    return null;
  }

  async subscribe(selector, expiresAt, stack) {
    const s = normalizeSelector(selector);
    const body = JSON.stringify({ selector: s, expiresAt, stack });
    console.log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/subscribe`, body);
    await this.fetch(`${this.url}/subscribe`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'x-vfs-id': stack[stack.length - 1] },
      signal: this.signal || AbortSignal.timeout(5000),
      body,
    }).then(r => console.log(`[MeshLink ${this.neighborId}] <- ${r.status} ${this.url}/subscribe`))
      .catch(() => {});
  }

  async notify(selector, payload, stack = []) {
    const s = normalizeSelector(selector);
    this._pulseCount++;
    this.lastPulse = Date.now();
    const body = JSON.stringify({ selector: s, payload, stack });
    console.log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/notify`, s.path);
    await this.fetch(`${this.url}/notify`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      signal: this.signal || AbortSignal.timeout(5000),
      body,
    }).then(r => console.log(`[MeshLink ${this.neighborId}] <- ${r.status} ${this.url}/notify`))
      .catch(() => {});
  }
}

/**
 * ReverseConnection: A pipe reached by replying to a neighbor's pending /listen poll.
 * Encapsulates BOTH the server-side pool management and the client-side poll loop.
 */
class ReverseConnection extends Connection {
  constructor(neighborId, mesh, options = {}) {
    super(neighborId);
    this.mesh = mesh;
    this.vfs = mesh.vfs;
    this.instanceId = options.instanceId || Math.random().toString(36).slice(2, 8);
    this.reachability = 'REVERSE';
    
    // Server-side state
    this.pool = [];
    this.queue = [];
    this.replies = new Map();
    
    // Client-side state
    this.abortController = new AbortController();
  }

  /**
   * SERVER SIDE: Handle an incoming /listen poll.
   */
  addScanner(res, replyTo = null, stream = null) {
    // CRITICAL ASSERTION: No hidden errors. 
    // If a node tries to open a second poll line, it's a protocol violation or zombie.
    if (this.pool.length > 0) {
      const msg = `CRITICAL MESH VIOLATION: Duplicate /listen received for peer ${this.neighborId}. Previous poll is still active. This indicates a 'Zombie' process or multiple tabs sharing the same ID.`;
      console.error(msg);
      res.writeHead(409, { 'Content-Type': 'text/plain' });
      res.end(msg);
      throw new Error(msg);
    }

    // Handle asynchronous read/spy replies via the tunnel
    if (replyTo && stream) {
      const resolve = this.replies.get(replyTo);
      if (resolve) {
        this.replies.delete(replyTo);
        resolve(stream, new Map(stream.length ? [['Content-Length', stream.length.toString()]] : []));
      }
    }

    // Flush any pending commands immediately
    if (this.queue.length > 0) {
      const cmd = this.queue.shift();
      console.log(`[ReverseConn ${this.neighborId}] Delivering queued command ${cmd.op || cmd.type} immediately.`);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(cmd));
      return;
    }

    // Otherwise, park the response in the pool
    this.pool.push({ res });
    res.on('close', () => {
      const idx = this.pool.findIndex((item) => item.res === res);
      if (idx !== -1) this.pool.splice(idx, 1);
    });
  }

  /**
   * SERVER SIDE: Send a command or notification by consuming a pooled /listen response.
   */
  _dispatch(command) {
    console.log(`[ReverseConn ${this.neighborId}] Dispatching command ${command.op || command.type}`);
    if (this.pool.length > 0) {
      const { res } = this.pool.shift();
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(command));
      return true;
    }
    this.queue.push(command);
    return false;
  }

  async read(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.replies.delete(requestId);
        reject(new Error(`Reverse read timeout for ${selector.path || selector}`));
      }, Math.max(0, expiresAt - Date.now()));

      this.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this._dispatch({ 
        type: 'COMMAND', 
        op: 'READ', 
        id: requestId, 
        selector, 
        stack, 
        resolutionStack: context.resolutionStack || [],
        expiresAt 
    });
    return replyPromise.catch(() => null);
  }

  async spy(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.replies.delete(requestId);
        reject(new Error(`Reverse spy timeout for ${selector.path}`));
      }, expiresAt - Date.now());

      this.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this._dispatch({ 
        type: 'COMMAND', 
        op: 'SPY', 
        id: requestId, 
        selector, 
        stack, 
        resolutionStack,
        expiresAt 
    });
    return replyPromise.catch(() => null);
  }

  async subscribe(selector, expiresAt, stack) {
    this._dispatch({ type: 'COMMAND', op: 'SUB', selector, expiresAt, stack });
  }

  async notify(selector, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    this._dispatch({ type: 'NOTIFY', selector, payload, stack });
  }

  /**
   * CLIENT SIDE: Actively poll a visible neighbor to receive mesh commands.
   */
  async startPolling(baseUrl, fetch) {
    console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} starting Poll-based Receiver for ${baseUrl}`);
    let replyTo = null, stream = null;
    
    while (!this.abortController.signal.aborted) {
      console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- ENTERING LOOP ITERATION ---`);
      try {
        const headers = { 'x-vfs-peer-id': this.vfs.id };
        if (replyTo) headers['x-vfs-reply-to'] = replyTo;
        
        console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} -> WAITING for next command from ${baseUrl}`);
        const resp = await fetch(`${baseUrl}/listen`, { 
            method: 'POST', 
            headers, 
            signal: this.abortController.signal, 
            body: stream 
        });
        replyTo = null; stream = null;
        
        console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} <- GOT SOMETHING (status ${resp.status}) from ${baseUrl}`);
        if (resp.status === 200) {
          const cmdText = await resp.text();
          const cmd = JSON.parse(cmdText);
          console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} - STARTING execution of ${cmd.op || cmd.type}`);
          
          if (cmd.type === 'COMMAND') {
            const sel = cmd.selector ? Selector.fromObject(cmd.selector) : null;
            if (cmd.op === 'READ') {
              stream = await this.vfs.read(sel || cmd.cid, { stack: cmd.stack, resolutionStack: cmd.resolutionStack, expiresAt: cmd.expiresAt });
              replyTo = cmd.id;
            } else if (cmd.op === 'SPY') {
              stream = await this.vfs.spy(sel, { stack: cmd.stack, resolutionStack: cmd.resolutionStack, expiresAt: cmd.expiresAt });
              replyTo = cmd.id;
            } else if (cmd.op === 'SUB') {
              this.mesh.addInterest(cmd.stack[0] || 'unknown', sel, cmd.expiresAt, cmd.stack);
            }
          } else if (cmd.type === 'NOTIFY') {
            const s = Selector.fromObject(cmd.selector);
            this.mesh.notify(s, cmd.payload, cmd.stack);
          }
          console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- FINISHED command. Looping. ---`);
        } else if (resp.status === 409) {
            const errText = await resp.text();
            throw new Error(`CRITICAL: Remote node rejected our poll: ${errText}`);
        } else { 
          if (resp.status !== 204) console.log(`[ReverseConn ${this.neighborId}] poll non-200 response: ${resp.status}`);
          console.log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- IDLE. Looping. ---`);
          await new Promise(r => setTimeout(r, 1000)); 
        }
      } catch (err) { 
        if (this.abortController.signal.aborted) break; 
        console.error(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} !!! POLL LOOP CRASHED !!!`, err.message);
        throw err; // Visible Error Requirement
      }
    }
  }

  stop() {
    this.abortController.abort();
    for (const { res } of this.pool) {
      try { res.end(); } catch (e) {}
    }
    this.pool = [];
  }
}

/**
 * MeshLink: Direct Neighbor Registry and Recursive Bread-crumb Router.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.instanceId = Math.random().toString(36).slice(2, 8);
    this.fetch = options.fetch || globalThis.fetch.bind(globalThis);
    this.localUrl = options.localUrl;
    console.log(`[MeshLink ${this.vfs.id}] Instance ${this.instanceId} created.`);
    this.peers = new Map(); // neighborId -> Connection
    this.connecting = new Set();
    this.neighborUrls = neighborUrls.map((u) => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.interests = new Map();

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

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    
    // Paint the local interest field
    const topic = JSON.stringify(s);
    if (!this.interests.has(topic)) {
        this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null, localExpiresAt: 0 });
    }
    const entry = this.interests.get(topic);
    entry.localExpiresAt = Math.max(entry.localExpiresAt, expiresAt);
    console.log(`[MeshLink ${this.vfs.id}] Local interest in ${s.path} recorded (expiresAt: ${expiresAt})`);
    
    // Propagate the interest (Painting the mesh)
    const nextStack = [...stack, this.vfs.id];
    for (const conn of this.peers.values()) {
      if (!nextStack.includes(conn.neighborId)) {
        console.log(`[MeshLink ${this.vfs.id}] -> Propagating local interest in ${s.path} to peer ${conn.neighborId}`);
        conn.subscribe(s, expiresAt, nextStack).catch(() => {});
      }
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const s = normalizeSelector(selector);
    const topic = JSON.stringify(s);
    console.log(`[MeshLink ${this.vfs.id}] addInterest: ${s.path} from ${neighborId} (stack: ${stack})`);
    if (!this.interests.has(topic)) {
      this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null, localExpiresAt: 0 });
    }
    
    const entry = this.interests.get(topic);
    const isNewNeighbor = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);

    // IMMEDIATE REPLY PHASE: 
    // 1. If we have a cached last value from the mesh, push it.
    if (isNewNeighbor && entry.lastValue) {
      const conn = this.peers.get(neighborId);
      if (conn) {
          console.log(`[MeshLink ${this.vfs.id}] -> Pushing cached lastValue for ${s.path} to ${neighborId}`);
          conn.notify(s, entry.lastValue, [this.vfs.id]).catch(() => {});
      }
    }

    // 2. If it's sys/schema, we MUST also contribute our own local catalog
    if (isNewNeighbor && s.path === 'sys/schema') {
        const conn = this.peers.get(neighborId);
        if (conn) {
            const catalog = this.vfs.getCatalog();
            console.log(`[MeshLink ${this.vfs.id}] -> Contributing local catalog to ${neighborId} (${Object.keys(catalog.catalog).length} ops)`);
            conn.notify(s, { type: 'CATALOG_ANNOUNCEMENT', ...catalog }, [this.vfs.id]).catch(() => {});
        }
    }

    // PROPAGATION PHASE:
    if (isNewNeighbor) {
      const nextStack = [...stack, this.vfs.id];
      for (const conn of this.peers.values()) {
        if (!nextStack.includes(conn.neighborId) && conn.neighborId !== neighborId) {
          console.log(`[MeshLink ${this.vfs.id}] -> Propagating interest in ${s.path} from ${neighborId} to ${conn.neighborId}`);
          conn.subscribe(s, expiresAt, nextStack).catch(() => {});
        }
      }
    }
  }

  notify(selector, payload, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    const nextStack = [...stack, this.vfs.id];

    console.log(`[MeshLink ${this.vfs.id}] notify: ${s.path} from stack ${stack}`);

    for (const entry of this.interests.values()) {
      if (this._matches(entry.selector, s)) {
        entry.lastValue = payload;
        for (const [neighborId, expiry] of entry.subs.entries()) {
          if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
          if (stack.includes(neighborId)) continue;
          
          const conn = this.peers.get(neighborId);
          if (conn) {
              console.log(`[MeshLink ${this.vfs.id}] -> Forwarding notification for ${s.path} to ${neighborId}`);
              conn.notify(s, payload, nextStack).catch(() => {});
          }
        }
      }
    }
  }

  _matches(sub, event) {
    if (!sub || !event) return false;
    if (sub.path === event.path) {
      const subParams = sub.parameters || {};
      if (Object.keys(subParams).length === 0) return true;
      for (const [k, v] of Object.entries(subParams)) {
        if (JSON.stringify(v) !== JSON.stringify(event.parameters?.[k])) return false;
      }
      return true;
    }
    return false;
  }

  async start() {
    this.vfs.mesh = this;
    for (const url of this.neighborUrls) await this.addPeer(url);
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    console.log(`[MeshLink ${this.vfs.id}] addPeer startup: Attempting connection to ${url}`);
    if (this.connecting.has(url)) {
        console.log(`[MeshLink ${this.vfs.id}] addPeer: Already connecting to ${url}, skipping.`);
        return;
    }
    this.connecting.add(url);
    try {
      console.log(`[MeshLink ${this.vfs.id}] -> POST ${url}/register (vfsId: ${this.vfs.id})`);
      const resp = await this.fetch(`${url}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: this.vfs.id, url: this.localUrl }),
        signal: this.abortController.signal || AbortSignal.timeout(5000),
      });
      console.log(`[MeshLink ${this.vfs.id}] <- ${resp.status} ${url}/register`);
      if (resp.ok) {
        const info = await resp.json();
        console.log(`[MeshLink ${this.vfs.id}] Registration successful for ${url}:`, info);
        if (info.id && info.id !== this.vfs.id) {
          if (!this.peers.has(info.id)) {
            const conn = new ForwardConnection(info.id, url, this.fetch, { 
                localUrl: this.localUrl, 
                signal: this.abortController.signal,
                reachability: info.reachability
            });
            this._setPeer(info.id, conn);
            console.log(`[MeshLink ${this.vfs.id}] Peer added to registry: ${info.id} (reachability: ${info.reachability})`);
            this.notify(new Selector('sys/topo'), { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
            for (const entry of this.interests.values()) {
              let maxExp = entry.localExpiresAt || 0;
              for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
              if (maxExp > Date.now()) {
                conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(()=>{});
              }
            }
          }
          // Browser Rule: If we have no local URL, we MUST poll for mesh events
          if (info.reachability === 'REVERSE' || !this.localUrl) {
            // Unify: Create a ReverseConnection for the poll link
            const pollerId = `${info.id}-poller`;
            if (!this.peers.has(pollerId)) {
                console.log(`[MeshLink ${this.vfs.id}] Starting reverse poll line for ${info.id}`);
                const poller = new ReverseConnection(pollerId, this, { instanceId: this.instanceId });
                this._setPeer(pollerId, poller);
                poller.startPolling(url, this.fetch);
            }
          }
          return info.id;
        }
      } else {
        const errText = await resp.text().catch(() => 'no body');
        console.warn(`[MeshLink ${this.vfs.id}] Handshake rejected by ${url} (Status ${resp.status}): ${errText}`);
      }
    } catch (e) { 
        const msg = `Handshake failed for ${url}: ${e.message} (Type: ${e.name})`;
        console.error(`[MeshLink ${this.vfs.id}] ${msg}`);
    } finally { this.connecting.delete(url); }
    return null;
  }

  async probeDirectReachability(url) {
    if (!url) return false;
    try {
      const target = `${url.replace(/\/$/, '')}/health`;
      const resp = await this.fetch(target, {
        signal: AbortSignal.timeout(1000),
      });
      const ok = resp.ok;
      console.log(`[MeshLink ${this.vfs.id}] Probing ${target} -> ${ok ? 'DIRECT' : 'REVERSE'}`);
      return ok;
    } catch (e) {
      console.log(`[MeshLink ${this.vfs.id}] Probing ${url} -> REVERSE (Error: ${e.message})`);
      return false;
    }
  }

  /**
   * SERVER SIDE: Entry point for a hidden peer calling /listen.
   */
  registerReversePeer(neighborId, res, replyTo = null, stream = null) {
    console.log(`[MeshLink ${this.vfs.id}] registerReversePeer: ${neighborId} (replyTo: ${replyTo || 'none'})`);
    if (!this.peers.has(neighborId)) {
      const conn = new ReverseConnection(neighborId, this, { instanceId: this.instanceId });
      this._setPeer(neighborId, conn);
      this.notify(new Selector('sys/topo'), { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
      for (const entry of this.interests.values()) {
        let maxExp = entry.localExpiresAt || 0;
        for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
        if (maxExp > Date.now()) {
          conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(()=>{});
        }
      }
    }
    const conn = this.peers.get(neighborId);
    if (!(conn instanceof ReverseConnection)) {
        throw new Error(`CRITICAL PROTOCOL VIOLATION: registerReversePeer called for ${neighborId} but connection is ${conn.constructor.name}`);
    }
    conn.addScanner(res, replyTo, stream);
  }

  async read(target, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    
    const targetConns = [...this.peers.values()].filter(c => !stack.includes(c.neighborId));
    if (targetConns.length === 0) return null;

    const fetchPromises = targetConns.map(async (conn) => {
      console.log(`[MeshLink ${this.vfs.id}] Requesting ${target.path || target} from peer: ${conn.neighborId}`);
      const resp = await conn.read(target, { ...context, stack, resolutionStack, expiresAt });
      if (resp) return resp;
      throw new Error('Conn failed');
    });
    try { 
        return await Promise.any(fetchPromises); 
    } catch (e) { 
        const msg = `Mesh-wide Read Failure for ${target.path || target}: ${e.message}`;
        console.error(`[MeshLink ${this.vfs.id}] ${msg}`);
        throw new Error(msg);
    } finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    
    const nextStack = stack.includes(this.vfs.id) ? stack : [...stack, this.vfs.id];
    const targetConns = [...this.peers.values()].filter(c => !nextStack.includes(c.neighborId));
    
    if (targetConns.length === 0) return null;
    const fetchPromises = targetConns.map(async (conn) => { try { return await conn.spy(s, { ...context, stack: nextStack, expiresAt }); } catch (err) { return null; } });
    try {
      const results = await Promise.allSettled(fetchPromises);
      const validStreams = results.filter(r => r.status === 'fulfilled' && r.value).map(r => r.value);
      if (validStreams.length === 0) return null;
      return new ReadableStream({
        async start(controller) {
          for (const s of validStreams) {
            const reader = s.getReader();
            try { while (true) { const { done, value } = await reader.read(); if (done) break; controller.enqueue(value); } } finally { reader.releaseLock(); }
          }
          controller.close();
        },
      });
    } finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  stop() { 
    this.abortController.abort(); 
    clearInterval(this._ppsInterval); 
    for (const conn of this.peers.values()) {
        if (conn instanceof ReverseConnection) conn.stop();
    }
    this.peers.clear(); 
  }
}

export class MeshLink extends MeshLinkBase {}
