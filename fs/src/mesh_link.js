import crypto from 'crypto';
import { normalizeSelector } from './vfs_core.js';

/**
 * Peer: Abstract interface for any entity we can request data from.
 */
class Peer {
  constructor(id) {
    this.id = id;
    this.reachability = 'UNKNOWN'; // 'DIRECT' or 'REVERSE'
    this.lastPulse = 0;
    this.pps = 0;
    this._pulseCount = 0;
  }

  _tickPPS() {
    this.pps = this._pulseCount;
    this._pulseCount = 0;
  }

  async read(path, parameters, stack) {
    throw new Error('Not implemented');
  }
  async spy(path, parameters, stack) {
    throw new Error('Not implemented');
  }
  async subscribe(selector, expiresAt, stack) {
    throw new Error('Not implemented');
  }
  async notify(selector, payload, stack) {
    throw new Error('Not implemented');
  }
}

/**
 * StaticPeer: A peer reachable via a stable HTTP URL.
 */
class StaticPeer extends Peer {
  constructor(id, url, fetch, options = {}) {
    super(id);
    this.url = url.replace(/\/$/, '');
    this.fetch = fetch;
    this.localUrl = options.localUrl;
    this.signal = options.signal;
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = {
      'Content-Type': 'application/json',
      'x-vfs-id': stack[0],
    };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const resp = await this.fetch(`${this.url}/read`, {
      method: 'POST',
      headers,
      signal: this.signal,
      body: JSON.stringify({ path, parameters, stack, expiresAt }),
    });

    if (resp.ok && resp.body) {
      return { body: resp.body, headers: resp.headers };
    }
    return null;
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = {
      'Content-Type': 'application/json',
      'x-vfs-id': stack[0],
    };
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
      headers: {
        'Content-Type': 'application/json',
        'x-vfs-id': stack[stack.length - 1],
      },
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
 * ReversePeer: A peer reached by replying to a pending /listen request.
 */
class ReversePeer extends Peer {
  constructor(id, registry) {
    super(id);
    this.registry = registry;
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.registry.replies.delete(requestId);
        const err = new Error(`Reverse read timeout for ${path} from peer ${this.id}`);
        err.code = 'ETIMEDOUT';
        reject(err);
      }, Math.max(0, expiresAt - Date.now()));

      this.registry.replies.set(requestId, (stream, headers = new Map()) => {
        clearTimeout(timeout);
        resolve({ body: stream, headers: headers });
      });
    });

    this.registry.dispatch(this.id, {
        type: 'COMMAND',
        op: 'READ',
        id: requestId,
        path,
        parameters,
        stack,
        expiresAt,
    });
    
    return replyPromise.catch(err => {
        if (err.code === 'ETIMEDOUT') return null;
        throw err;
    });
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
        resolve({ body: stream, headers: headers });
      });
    });

    this.registry.dispatch(this.id, {
        type: 'COMMAND',
        op: 'SPY',
        id: requestId,
        path,
        parameters,
        stack,
        expiresAt,
    });
    
    return replyPromise.catch(err => {
        if (err.code === 'ETIMEDOUT') return null;
        throw err;
    });
  }

  async subscribe(topic, expiresAt, stack) {
    this.registry.dispatch(this.id, {
      type: 'COMMAND',
      op: 'SUB',
      selector: topic,
      expiresAt,
      stack,
    });
  }

  async notify(topic, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    this.registry.dispatch(this.id, {
      type: 'NOTIFY',
      selector: topic,
      payload,
      stack,
    });
  }
}

/**
 * MeshLink: Peer Registry and Recursive Bread-crumb Router.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.fetch = options.fetch || globalThis.fetch.bind(globalThis);
    this.localUrl = options.localUrl;
    this.peers = new Map();
    this.connecting = new Set();
    this.listeningTo = new Set(); 
    this.neighborUrls = neighborUrls.map((u) => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.interests = new Map(); 
    this.lastNotify = new Map();
    this.notifyHistory = [];

    this.commandQueues = new Map(); // peerId -> Array<Command>
    this.listenerPools = new Map(); // peerId -> Array<{res, req}>

    this.reverseRegistry = {
      replies: new Map(),
      dispatch: (peerId, command) => {
        const pool = this.listenerPools.get(peerId);
        if (pool && pool.length > 0) {
          const { res } = pool.shift(); 
          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify(command));
          return true;
        }
        if (!this.commandQueues.has(peerId)) this.commandQueues.set(peerId, []);
        this.commandQueues.get(peerId).push(command);
        return false;
      },
    };

    this._ppsInterval = setInterval(() => {
      for (const peer of this.peers.values()) peer._tickPPS();
    }, 1000);
    if (this._ppsInterval.unref) this._ppsInterval.unref();

    // Heartbeats REMOVED.
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    const neighbors = [...this.peers.values()].filter(p => !stack.includes(p.id));
    const newStack = [...stack, this.vfs.id];
    for (const peer of neighbors) {
      try {
        peer.subscribe(selector, expiresAt, newStack).catch(() => {});
      } catch (e) {}
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const topicString = JSON.stringify(normalizeSelector(selector));
    if (!this.interests.has(topicString))
      this.interests.set(topicString, { selector, subs: new Map() });
    const entry = this.interests.get(topicString);
    const isNewSubscriber = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);
    
    // Immediate state push for Topology/Schema subscriptions
    if (isNewSubscriber) {
        if (selector.path === 'sys/topo') {
            this.notify(selector, {
                type: 'TOPOLOGY_UPDATE',
                peer: this.vfs.id,
                neighbors: [...this.peers.values()].map(p => ({ id: p.id, reachability: p.reachability }))
            });
        } else if (selector.path === 'sys/schema') {
            this.vfs.spy('sys/schema').then(stream => {
               // Full catalog push would go here. For now, trigger a general notify if available.
            });
        }
    }

    if (entry.subs.size === 1 && isNewSubscriber) {
      this.subscribe(selector, expiresAt, [...stack, neighborId]).catch(() => {});
    }
  }

  notify(selector, payload, stack = []) {
    if (stack.includes(this.vfs.id)) return;
    const newStack = [...stack, this.vfs.id];
    const s = normalizeSelector(selector);
    const topicString = JSON.stringify(s);
    this.lastNotify.set(topicString, payload);

    for (const [topic, entry] of this.interests.entries()) {
      if (this._matches(entry.selector, s)) {
        for (const [neighborId, expiry] of entry.subs.entries()) {
          if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
          if (stack.includes(neighborId)) continue;
          const peer = this.peers.get(neighborId);
          if (peer) peer.notify(s, payload, newStack).catch(() => {});
        }
      }
    }
  }

  _matches(sub, event) {
    if (!sub || !event) return false;
    if (sub.path === event.path) {
      const subKeys = Object.keys(sub.parameters || {});
      if (subKeys.length === 0) return true;
      for (const key of subKeys) {
        if (JSON.stringify(sub.parameters[key]) !== JSON.stringify(event.parameters?.[key])) return false;
      }
      return true;
    }
    return false;
  }

  async start() {
    this.vfs.mesh = this;
    for (const url of this.neighborUrls) {
      await this.addPeer(url);
    }
  }

  async listenLoop(baseUrl) {
    if (this.listeningTo.has(baseUrl)) return;
    this.listeningTo.add(baseUrl);
    let replyTo = null;
    let stream = null;
    try {
        while (!this.abortController.signal.aborted) {
            try {
                const headers = { 'x-vfs-peer-id': this.vfs.id };
                if (replyTo) headers['x-vfs-reply-to'] = replyTo;
                const resp = await this.fetch(`${baseUrl}/listen`, {
                    method: 'POST',
                    headers,
                    signal: this.abortController.signal,
                    body: stream,
                });
                replyTo = null; stream = null;
                if (resp.status === 200) {
                    const command = await resp.json();
                    if (command.type === 'COMMAND') {
                        if (command.op === 'READ') {
                            stream = await this.vfs.read(command.path, command.parameters, { stack: command.stack, expiresAt: command.expiresAt });
                            replyTo = command.id;
                        } else if (command.op === 'SPY') {
                            stream = await this.vfs.spy(command.path, command.parameters, { stack: command.stack, expiresAt: command.expiresAt });
                            replyTo = command.id;
                        } else if (command.op === 'SUB') {
                            this.addInterest(command.stack[0] || 'unknown', command.selector, command.expiresAt, command.stack);
                        }
                    } else if (command.type === 'NOTIFY') {
                        this.notify(command.selector, command.payload, command.stack);
                    }
                } else if (resp.status === 204) {
                } else if (resp.status === 429) {
                    await new Promise(r => setTimeout(r, 5000));
                } else {
                    await new Promise(r => setTimeout(r, 5000));
                }
            } catch (err) {
                if (this.abortController.signal.aborted) break;
                await new Promise(r => setTimeout(r, 5000));
            }
        }
    } finally {
        this.listeningTo.delete(baseUrl);
    }
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
        const remoteId = info.id;
        if (remoteId && remoteId !== this.vfs.id) {
          if (!this.peers.has(remoteId)) {
            const peer = new StaticPeer(remoteId, url, this.fetch, { localUrl: this.localUrl, signal: this.abortController.signal });
            peer.reachability = 'DIRECT';
            this.peers.set(remoteId, peer);
            // Notify mesh of topology change
            this.notify({ path: 'sys/topo' }, {
                type: 'TOPOLOGY_UPDATE',
                peer: this.vfs.id,
                neighbors: [...this.peers.values()].map(p => ({ id: p.id, reachability: p.reachability }))
            });
          }
          if (info.reachability === 'REVERSE') this.listenLoop(url);
          return remoteId;
        }
      }
    } catch (e) {
    } finally {
      this.connecting.delete(url);
    }
  }

  async testReachability(url) {
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

  registerReversePeer(peerId, res, replyTo = null, stream = null) {
    if (replyTo && stream) {
      const resolve = this.reverseRegistry.replies.get(replyTo);
      if (resolve) {
        this.reverseRegistry.replies.delete(replyTo);
        const headers = new Map();
        if (stream.length) headers.set('Content-Length', stream.length.toString());
        resolve(stream, headers);
      }
    }
    if (!this.peers.has(peerId)) {
      const peer = new ReversePeer(peerId, this.reverseRegistry);
      peer.reachability = 'REVERSE';
      this.peers.set(peerId, peer);
      // Notify mesh of topology change
      this.notify({ path: 'sys/topo' }, {
          type: 'TOPOLOGY_UPDATE',
          peer: this.vfs.id,
          neighbors: [...this.peers.values()].map(p => ({ id: p.id, reachability: p.reachability }))
      });
    }
    if (!this.listenerPools.has(peerId)) this.listenerPools.set(peerId, []);
    const pool = this.listenerPools.get(peerId);
    if (pool.length >= 10) {
        res.writeHead(429); res.end('Pool Saturated'); return;
    }
    const queue = this.commandQueues.get(peerId);
    if (queue && queue.length > 0) {
        const cmd = queue.shift();
        res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(cmd)); return;
    }
    pool.push({ res });
    res.on('close', () => {
        const idx = pool.findIndex(item => item.res === res);
        if (idx !== -1) pool.splice(idx, 1);
    });
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    if (targetPeers.length === 0) return null;
    const fetchPromises = targetPeers.map(async (peer) => {
      try {
        const resp = await peer.read(path, parameters, { stack: newStack, expiresAt });
        if (resp) return resp;
      } catch (err) {}
      throw new Error('Peer failed');
    });
    try {
      return await Promise.any(fetchPromises);
    } catch (e) {
      return null;
    } finally {
      for (const p of fetchPromises) p.catch(() => {});
    }
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    if (targetPeers.length === 0) return null;
    const fetchPromises = targetPeers.map(async (peer) => {
      try {
        return await peer.spy(path, parameters, { stack: newStack, expiresAt });
      } catch (err) { return null; }
    });
    try {
      const results = await Promise.allSettled(fetchPromises);
      const validStreams = results.filter(r => r.status === 'fulfilled' && r.value).map(r => r.value);
      if (validStreams.length === 0) return null;
      return new ReadableStream({
        async start(controller) {
          for (const stream of validStreams) {
            const reader = stream.getReader();
            try {
              while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                controller.enqueue(value);
              }
            } finally { reader.releaseLock(); }
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
    for (const pool of this.listenerPools.values()) {
        for (const { res } of pool) { try { res.end(); } catch(e){} }
    }
    this.listenerPools.clear();
  }
}

export class MeshLink extends MeshLinkBase {}
