import crypto from 'crypto';
import { normalizeSelector } from './vfs_core.js';

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

  async read(path, parameters, stack) { throw new Error('Not implemented'); }
  async spy(path, parameters, stack) { throw new Error('Not implemented'); }
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
    this.reachability = 'DIRECT';
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 'Content-Type': 'application/json', 'x-vfs-id': stack[0] };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const resp = await this.fetch(`${this.url}/read`, {
      method: 'POST',
      headers,
      signal: this.signal,
      body: JSON.stringify({ path, parameters, stack, expiresAt }),
    });

    if (resp.ok && resp.body) return { body: resp.body, headers: resp.headers };
    return null;
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 'Content-Type': 'application/json', 'x-vfs-id': stack[0] };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const resp = await this.fetch(`${this.url}/spy`, {
      method: 'POST',
      headers,
      signal: this.signal,
      body: JSON.stringify({ path, parameters, stack, expiresAt }),
    });

    if (resp.ok && resp.body) return resp.body;
    return null;
  }

  async subscribe(topic, expiresAt, stack) {
    await this.fetch(`${this.url}/subscribe`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'x-vfs-id': stack[stack.length - 1] },
      signal: this.signal,
      body: JSON.stringify({ selector: topic, expiresAt, stack }),
    }).catch(() => {});
  }

  async notify(topic, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    await this.fetch(`${this.url}/notify`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      signal: this.signal,
      body: JSON.stringify({ selector: topic, payload, stack }),
    }).catch(() => {});
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

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.registry.replies.delete(requestId);
        reject(new Error(`Reverse read timeout for ${path}`));
      }, Math.max(0, expiresAt - Date.now()));

      this.registry.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this.registry.dispatch(this.neighborId, { type: 'COMMAND', op: 'READ', id: requestId, path, parameters, stack, expiresAt });
    return replyPromise.catch(() => null);
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.registry.replies.delete(requestId);
        reject(new Error(`Reverse spy timeout for ${path}`));
      }, expiresAt - Date.now());

      this.registry.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers });
      });
    });

    this.registry.dispatch(this.neighborId, { type: 'COMMAND', op: 'SPY', id: requestId, path, parameters, stack, expiresAt });
    return replyPromise.catch(() => null);
  }

  async subscribe(topic, expiresAt, stack) {
    this.registry.dispatch(this.neighborId, { type: 'COMMAND', op: 'SUB', selector: topic, expiresAt, stack });
  }

  async notify(topic, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    this.registry.dispatch(this.neighborId, { type: 'NOTIFY', selector: topic, payload, stack });
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
    if (stack.length > 0) {
      this.addInterest(stack[stack.length - 1], s, expiresAt, stack);
    }

    const nextStack = [...stack, this.vfs.id];
    for (const conn of this.peers.values()) {
      if (!stack.includes(conn.neighborId)) {
        conn.subscribe(s, expiresAt, nextStack).catch(() => {});
      }
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const topic = JSON.stringify(normalizeSelector(selector));
    if (!this.interests.has(topic))
      this.interests.set(topic, { selector, subs: new Map(), lastValue: null });
    
    const entry = this.interests.get(topic);
    const isNew = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);

    if (isNew && entry.lastValue) {
      const conn = this.peers.get(neighborId);
      if (conn) conn.notify(selector, entry.lastValue, [this.vfs.id]).catch(() => {});
    }

    if (isNew && entry.subs.size === 1) {
      this.subscribe(selector, expiresAt, [...stack, neighborId]).catch(() => {});
    }
  }

  notify(selector, payload, stack = []) {
    if (stack.includes(this.vfs.id)) return;
    const s = normalizeSelector(selector);
    const nextStack = [...stack, this.vfs.id];

    for (const entry of this.interests.values()) {
      if (this._matches(entry.selector, s)) {
        entry.lastValue = payload;
        for (const [neighborId, expiry] of entry.subs.entries()) {
          if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
          if (stack.includes(neighborId)) continue;
          const conn = this.peers.get(neighborId);
          if (conn) conn.notify(s, payload, nextStack).catch(() => {});
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
    let replyTo = null, stream = null;
    try {
      while (!this.abortController.signal.aborted) {
        try {
          const headers = { 'x-vfs-peer-id': this.vfs.id };
          if (replyTo) headers['x-vfs-reply-to'] = replyTo;
          const resp = await this.fetch(`${baseUrl}/listen`, { method: 'POST', headers, signal: this.abortController.signal, body: stream });
          replyTo = null; stream = null;
          if (resp.status === 200) {
            const cmd = await resp.json();
            if (cmd.type === 'COMMAND') {
              if (cmd.op === 'READ') {
                stream = await this.vfs.read(cmd.path, cmd.parameters, { stack: cmd.stack, expiresAt: cmd.expiresAt });
                replyTo = cmd.id;
              } else if (cmd.op === 'SPY') {
                stream = await this.vfs.spy(cmd.path, cmd.parameters, { stack: cmd.stack, expiresAt: cmd.expiresAt });
                replyTo = cmd.id;
              } else if (cmd.op === 'SUB') {
                this.addInterest(cmd.stack[0] || 'unknown', cmd.selector, cmd.expiresAt, cmd.stack);
              }
            } else if (cmd.type === 'NOTIFY') {
              this.notify(cmd.selector, cmd.payload, cmd.stack);
            }
          } else { await new Promise(r => setTimeout(r, 5000)); }
        } catch (err) { if (this.abortController.signal.aborted) break; await new Promise(r => setTimeout(r, 5000)); }
      }
    } finally { this.listeningTo.delete(baseUrl); }
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    if (this.connecting.has(url)) return;
    this.connecting.add(url);
    try {
      const resp = await this.fetch(`${url}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: this.vfs.id, url: this.localUrl }),
        signal: this.abortController.signal,
      });
      if (resp.ok) {
        const info = await resp.json();
        if (info.id && info.id !== this.vfs.id) {
          if (!this.peers.has(info.id)) {
            const conn = new ForwardConnection(info.id, url, this.fetch, { localUrl: this.localUrl, signal: this.abortController.signal });
            this.peers.set(info.id, conn);
            this.notify({ path: 'sys/topo' }, { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
          }
          if (info.reachability === 'REVERSE') this.listenLoop(url);
          return info.id;
        }
      }
    } catch (e) { } finally { this.connecting.delete(url); }
  }

  async probeDirectReachability(url) {
    if (!url) return false;
    try {
      const resp = await this.fetch(`${url.replace(/\/$/, '')}/health`, {
        signal: AbortSignal.timeout(1000),
      });
      return resp.ok;
    } catch (e) {
      return false;
    }
  }

  registerReversePeer(neighborId, res, replyTo = null, stream = null) {
    if (replyTo && stream) {
      const resolve = this.reverseRegistry.replies.get(replyTo);
      if (resolve) { this.reverseRegistry.replies.delete(replyTo); resolve(stream, new Map(stream.length ? [['Content-Length', stream.length.toString()]] : [])); }
    }
    if (!this.peers.has(neighborId)) {
      const conn = new ReverseConnection(neighborId, this.reverseRegistry);
      this.peers.set(neighborId, conn);
      this.notify({ path: 'sys/topo' }, { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
    }
    if (!this.listenerPools.has(neighborId)) this.listenerPools.set(neighborId, []);
    const pool = this.listenerPools.get(neighborId);
    if (pool.length >= 10) { res.writeHead(429); res.end('Pool Saturated'); return; }
    const queue = this.commandQueues.get(neighborId);
    if (queue && queue.length > 0) { const cmd = queue.shift(); res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(cmd)); return; }
    pool.push({ res });
    res.on('close', () => { const idx = pool.findIndex(item => item.res === res); if (idx !== -1) pool.splice(idx, 1); });
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const nextStack = [...stack, this.vfs.id];
    const targetConns = [...this.peers.values()].filter(c => !stack.includes(c.neighborId));
    if (targetConns.length === 0) return null;
    const fetchPromises = targetConns.map(async (conn) => {
      const resp = await conn.read(path, parameters, { stack: nextStack, expiresAt });
      if (resp) return resp;
      throw new Error('Conn failed');
    });
    try { return await Promise.any(fetchPromises); } catch (e) { return null; } finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const nextStack = [...stack, this.vfs.id];
    const targetConns = [...this.peers.values()].filter(c => !stack.includes(c.neighborId));
    if (targetConns.length === 0) return null;
    const fetchPromises = targetConns.map(async (conn) => { try { return await conn.spy(path, parameters, { stack: nextStack, expiresAt }); } catch (err) { return null; } });
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
