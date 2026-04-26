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
 */
class ReverseConnection extends Connection {
  constructor(neighborId, registry) {
    super(neighborId);
    this.registry = registry;
    this.reachability = 'REVERSE';
  }

  async read(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.registry.replies.delete(requestId);
        reject(new Error(`Reverse read timeout for ${selector.path}`));
      }, Math.max(0, expiresAt - Date.now()));

      this.registry.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this.registry.dispatch(this.neighborId, { 
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
        this.registry.replies.delete(requestId);
        reject(new Error(`Reverse spy timeout for ${selector.path}`));
      }, expiresAt - Date.now());

      this.registry.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this.registry.dispatch(this.neighborId, { 
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
    this.registry.dispatch(this.neighborId, { type: 'COMMAND', op: 'SUB', selector, expiresAt, stack });
  }

  async notify(selector, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    this.registry.dispatch(this.neighborId, { type: 'NOTIFY', selector, payload, stack });
  }
}

/**
 * MeshLink: Direct Neighbor Registry and Recursive Bread-crumb Router.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.fetch = options.fetch || globalThis.fetch.bind(globalThis);
    this.localUrl = options.localUrl;
    this.peers = new Map(); // neighborId -> Connection
    this.connecting = new Set();
    this.listeningTo = new Set();
    this.neighborUrls = neighborUrls.map((u) => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.interests = new Map();
    this.commandQueues = new Map(); // neighborId -> Array<Command>
    this.listenerPools = new Map(); // neighborId -> Array<{res, req}>

    this.reverseRegistry = {
      replies: new Map(),
      dispatch: (neighborId, command) => {
        console.log(`[MeshLink ${this.vfs.id}] Dispatching command ${command.op || command.type} to ${neighborId}`);
        const pool = this.listenerPools.get(neighborId);
        if (pool && pool.length > 0) {
          const { res } = pool.shift();
          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify(command));
          return true;
        }
        if (!this.commandQueues.has(neighborId)) this.commandQueues.set(neighborId, []);
        this.commandQueues.get(neighborId).push(command);
        return false;
      },
    };

    this._ppsInterval = setInterval(() => {
      for (const conn of this.peers.values()) conn._tickPPS();
    }, 1000);
    if (this._ppsInterval.unref) this._ppsInterval.unref();
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    
    // Paint the local interest field
    const topic = JSON.stringify(s);
    if (!this.interests.has(topic)) {
        this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null });
    }
    
    // Propagate the interest (Painting the mesh)
    const nextStack = [...stack, this.vfs.id];
    for (const conn of this.peers.values()) {
      if (!nextStack.includes(conn.neighborId)) {
        conn.subscribe(s, expiresAt, nextStack).catch(() => {});
      }
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const s = normalizeSelector(selector);
    const topic = JSON.stringify(s);
    if (!this.interests.has(topic)) {
      this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null });
    }
    
    const entry = this.interests.get(topic);
    const isNewNeighbor = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);

    console.log(`[MeshLink ${this.vfs.id}] Interest added for ${topic} from ${neighborId}`);

    // If we have a cached value, push it immediately to the new interested neighbor
    if (isNewNeighbor && entry.lastValue) {
      const conn = this.peers.get(neighborId);
      if (conn) conn.notify(s, entry.lastValue, [this.vfs.id]).catch(() => {});
    }

    // Propagate interest further if this is the first time we've seen interest in this topic
    if (isNewNeighbor && entry.subs.size === 1) {
      const nextStack = [...stack, this.vfs.id];
      for (const conn of this.peers.values()) {
        if (!nextStack.includes(conn.neighborId)) {
          conn.subscribe(s, expiresAt, nextStack).catch(() => {});
        }
      }
    }
  }

  notify(selector, payload, stack = []) {
    if (stack.includes(this.vfs.id)) return;
    const s = normalizeSelector(selector);
    const nextStack = [...stack, this.vfs.id];

    console.log(`[MeshLink ${this.vfs.id}] Received publication for ${s.path} (stack: ${stack})`);

    // Natural spreading through the interest field
    for (const entry of this.interests.values()) {
      if (this._matches(entry.selector, s)) {
        entry.lastValue = payload;
        for (const [neighborId, expiry] of entry.subs.entries()) {
          if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
          // Forward to neighbor UNLESS they are in the backflow stack
          if (stack.includes(neighborId)) continue;
          
          const conn = this.peers.get(neighborId);
          if (conn) {
              console.log(`[MeshLink ${this.vfs.id}] Forwarding pub for ${s.path} to neighbor ${neighborId}`);
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

  async listenLoop(baseUrl) {
    if (this.listeningTo.has(baseUrl)) return;
    this.listeningTo.add(baseUrl);
    console.log(`[MeshLink ${this.vfs.id}] Starting listenLoop for ${baseUrl}`);
    let replyTo = null, stream = null;
    try {
      while (!this.abortController.signal.aborted) {
        try {
          const headers = { 'x-vfs-peer-id': this.vfs.id };
          if (replyTo) headers['x-vfs-reply-to'] = replyTo;
          
          console.log(`[MeshLink ${this.vfs.id}] -> POST ${baseUrl}/listen (replyTo: ${replyTo || 'none'})`);
          const resp = await this.fetch(`${baseUrl}/listen`, { method: 'POST', headers, signal: this.abortController.signal, body: stream });
          replyTo = null; stream = null;
          
          console.log(`[MeshLink ${this.vfs.id}] <- ${resp.status} ${baseUrl}/listen`);
          if (resp.status === 200) {
            const cmdText = await resp.text();
            console.log(`[MeshLink ${this.vfs.id}]   - Raw Body:`, cmdText);
            const cmd = JSON.parse(cmdText);
            console.log(`[MeshLink ${this.vfs.id}] Received COMMAND ${cmd.op || cmd.type} from ${baseUrl}`);
            if (cmd.type === 'COMMAND') {
              const sel = cmd.selector;
              if ((cmd.op === 'READ' || cmd.op === 'SPY') && !sel) {
                console.error(`[MeshLink ${this.vfs.id}] Received COMMAND ${cmd.op} without selector`);
                continue;
              }
              if (cmd.op === 'READ') {
                stream = await this.vfs.read(sel, {
                  stack: cmd.stack,
                  resolutionStack: cmd.resolutionStack,
                  expiresAt: cmd.expiresAt,
                });
                replyTo = cmd.id;
              } else if (cmd.op === 'SPY') {
                stream = await this.vfs.spy(sel, {
                  stack: cmd.stack,
                  resolutionStack: cmd.resolutionStack,
                  expiresAt: cmd.expiresAt,
                });
                replyTo = cmd.id;
              } else if (cmd.op === 'SUB') {
                if (!sel) {
                    console.error(`[MeshLink ${this.vfs.id}] Received SUB without selector`);
                    continue;
                }
                this.addInterest(
                  cmd.stack[0] || 'unknown',
                  sel,
                  cmd.expiresAt,
                  cmd.stack
                );
              }
            } else if (cmd.type === 'NOTIFY') {
              if (!cmd.selector) {
                  console.error(`[MeshLink ${this.vfs.id}] Received NOTIFY without selector`);
                  continue;
              }
              const s = Selector.fromObject(cmd.selector);
              this.notify(s, cmd.payload, cmd.stack);
            }
          } else { 
            if (resp.status !== 204) console.log(`[MeshLink ${this.vfs.id}] listenLoop non-200 response: ${resp.status}`);
            await new Promise(r => setTimeout(r, 5000)); 
          }
        } catch (err) { 
          if (this.abortController.signal.aborted) break; 
          console.error(`[MeshLink ${this.vfs.id}] listenLoop error:`, err.message);
          await new Promise(r => setTimeout(r, 5000)); 
        }
      }
    } finally { this.listeningTo.delete(baseUrl); }
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    console.log(`[MeshLink ${this.vfs.id}] Attempting to add peer at ${url}`);
    if (this.connecting.has(url)) return;
    this.connecting.add(url);
    try {
      console.log(`[MeshLink ${this.vfs.id}] -> POST ${url}/register`);
      const resp = await this.fetch(`${url}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: this.vfs.id, url: this.localUrl }),
        signal: this.abortController.signal || AbortSignal.timeout(5000),
      });
      console.log(`[MeshLink ${this.vfs.id}] <- ${resp.status} ${url}/register`);
      console.log(`[MeshLink ${this.vfs.id}] Register response status: ${resp.status}`);
      if (resp.ok) {
        const info = await resp.json();
        console.log(`[MeshLink ${this.vfs.id}] Register info:`, info);
        if (info.id && info.id !== this.vfs.id) {
          if (!this.peers.has(info.id)) {
            const conn = new ForwardConnection(info.id, url, this.fetch, { 
                localUrl: this.localUrl, 
                signal: this.abortController.signal,
                reachability: info.reachability
            });
            this.peers.set(info.id, conn);
            console.log(`[MeshLink ${this.vfs.id}] Peer added: ${info.id}`);
            this.notify(new Selector('sys/topo'), { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
          }
          // Browser Rule: If we have no local URL, we MUST poll for mesh events
          if (info.reachability === 'REVERSE' || !this.localUrl) {
            this.listenLoop(url);
          }
          return info.id;
        }
      }
    } catch (e) { 
        if (e.name === 'AbortError' || e.name === 'TimeoutError' || e.message?.includes('aborted')) return null;
        const msg = `Handshake failed for ${url}: ${e.message}`;
        console.warn(`[MeshLink ${this.vfs.id}] ${msg}`);
        // Do not throw; handshaking is opportunistic and backgrounded
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

  registerReversePeer(neighborId, res, replyTo = null, stream = null) {
    console.log(`[MeshLink ${this.vfs.id}] registerReversePeer: ${neighborId} (replyTo: ${replyTo || 'none'})`);
    if (replyTo && stream) {
      const resolve = this.reverseRegistry.replies.get(replyTo);
      if (resolve) { this.reverseRegistry.replies.delete(replyTo); resolve(stream, new Map(stream.length ? [['Content-Length', stream.length.toString()]] : [])); }
    }
    if (!this.peers.has(neighborId)) {
      const conn = new ReverseConnection(neighborId, this.reverseRegistry);
      this.peers.set(neighborId, conn);
      this.notify(new Selector('sys/topo'), { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
    }
    if (!this.listenerPools.has(neighborId)) this.listenerPools.set(neighborId, []);
    const pool = this.listenerPools.get(neighborId);
    if (pool.length >= 10) { res.writeHead(429); res.end('Pool Saturated'); return; }
    const queue = this.commandQueues.get(neighborId);
    if (queue && queue.length > 0) { const cmd = queue.shift(); res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(cmd)); return; }
    pool.push({ res });
    res.on('close', () => { const idx = pool.findIndex(item => item.res === res); if (idx !== -1) pool.splice(idx, 1); });
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

  stop() { this.abortController.abort(); clearInterval(this._ppsInterval); for (const pool of this.listenerPools.values()) { for (const { res } of pool) { try { res.end(); } catch(e){} } } this.listenerPools.clear(); }
}

export class MeshLink extends MeshLinkBase {}
