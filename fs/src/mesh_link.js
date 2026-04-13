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
            'x-vfs-id': stack[0]
        };
        if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

        const resp = await this.fetch(`${this.url}/read`, {
            method: 'POST',
            headers,
            signal: this.signal,
            body: JSON.stringify({ path, parameters, stack, expiresAt })
        });

        if (resp.ok && resp.body) {
            return { body: resp.body, headers: resp.headers };
        }
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
            body: JSON.stringify({ path, parameters, stack, expiresAt })
        });

        if (resp.ok && resp.body) return resp.body;
        return null;
    }

    async subscribe(topic, expiresAt, stack) {
        await this.fetch(`${this.url}/subscribe`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'x-vfs-id': stack[stack.length-1] },
            signal: this.signal,
            body: JSON.stringify({ selector: topic, expiresAt, stack })
        }).catch(() => {});
    }

    async notify(topic, payload, stack = []) {
        this._pulseCount++;
        this.lastPulse = Date.now();
        await this.fetch(`${this.url}/notify`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            signal: this.signal,
            body: JSON.stringify({ selector: topic, payload, stack })
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
                reject(new Error(`Reverse read timeout for ${path}`));
            }, expiresAt - Date.now());

            this.registry.replies.set(requestId, (stream, headers = new Map()) => {
                clearTimeout(timeout);
                resolve({ body: stream, headers: headers });
            });
        });

        if (!this.registry.dispatch(this.id, { type: 'COMMAND', op: 'READ', id: requestId, path, parameters, stack, expiresAt })) {
            this.registry.replies.delete(requestId);
            return null;
        }
        return replyPromise;
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

        if (!this.registry.dispatch(this.id, { type: 'COMMAND', op: 'SPY', id: requestId, path, parameters, stack, expiresAt })) {
            this.registry.replies.delete(requestId);
            return null;
        }
        return replyPromise;
    }

    async subscribe(topic, expiresAt, stack) {
        this.registry.dispatch(this.id, { type: 'COMMAND', op: 'SUB', selector: topic, expiresAt, stack });
    }

    async notify(topic, payload, stack = []) {
        this._pulseCount++;
        this.lastPulse = Date.now();
        this.registry.dispatch(this.id, { type: 'COMMAND', op: 'NOTIFY', selector: topic, payload, stack });
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
    this.neighborUrls = neighborUrls.map(u => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    // Pub-Sub State
    this.interests = new Map(); // Normalized Topic JSON -> { selector, subs: Map<NeighborID, Expiry> }
    this.lastNotify = new Map();
    this.notifyHistory = [];

    this.reverseRegistry = {
        listeners: new Map(),
        replies: new Map(),
        dispatch: (peerId, command) => {
            const res = this.reverseRegistry.listeners.get(peerId);
            if (res) {
                this.reverseRegistry.listeners.delete(peerId);
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify(command));
                return true;
            }
            return false;
        }
    };

    // Auto-propagate local VFS events
    this.vfs.events.on('trace', (ev) => {
        const selector = ev.selector || { path: 'sys/trace', parameters: {} };
        this.notify(selector, { ...ev, peer: this.vfs.id, timestamp: Date.now() });
    });

    this._ppsInterval = setInterval(() => {
        for (const peer of this.peers.values()) peer._tickPPS();
    }, 1000);
    if (this._ppsInterval.unref) this._ppsInterval.unref();

    // Topology Heartbeat
    this._topoInterval = setInterval(() => {
        const neighbors = [...this.peers.values()].map(p => ({ id: p.id, reachability: p.reachability }));
        const selector = { path: 'sys/topo', parameters: { id: this.vfs.id } };
        this.notify(selector, { type: 'TOPOLOGY_UPDATE', peer: this.vfs.id, neighbors });
    }, 5000);
    if (this._topoInterval.unref) this._topoInterval.unref();
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
      const neighbors = [...this.peers.values()].filter(p => !stack.includes(p.id));
      const newStack = [...stack, this.vfs.id];
      for (const peer of neighbors) {
          try { peer.subscribe(selector, expiresAt, newStack).catch(() => {}); } catch (e) {}
      }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
      const topicString = JSON.stringify(normalizeSelector(selector));
      if (!this.interests.has(topicString)) this.interests.set(topicString, { selector, subs: new Map() });
      const entry = this.interests.get(topicString);
      const isNewTopic = entry.subs.size === 0;
      entry.subs.set(neighborId, expiresAt);
      for (const [id, expiry] of entry.subs.entries()) { if (Date.now() > expiry) entry.subs.delete(id); }

      if (isNewTopic) {
          this.subscribe(selector, expiresAt, [...stack, neighborId]).catch(() => {});
      }
  }

  notify(selector, payload, stack = []) {
      // Loop protection: If we've already seen this notification, stop.
      if (stack.includes(this.vfs.id)) return;
      
      const newStack = [...stack, this.vfs.id];
      const s = normalizeSelector(selector);
      const topicString = JSON.stringify(s);

      this.lastNotify.set(topicString, payload);
      
      // Store in history with the full traversal stack
      this.notifyHistory.push({ 
          selector: s, 
          payload: { ...payload, stack: newStack }, 
          t: Date.now() 
      });
      if (this.notifyHistory.length > 100) this.notifyHistory.shift();

      for (const [topic, entry] of this.interests.entries()) {
          if (this._matches(entry.selector, s)) {
              for (const [neighborId, expiry] of entry.subs.entries()) {
                  if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
                  
                  // Don't send back to the neighbor that just sent it to us
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
          if (subKeys.length === 0) return true; // Broad path match
          
          // Parameter subset match: event must have at least all parameters of sub
          for (const key of subKeys) {
              if (JSON.stringify(sub.parameters[key]) !== JSON.stringify(event.parameters?.[key])) return false;
          }
          return true;
      }
      return false;
  }

  async start() {
    this.vfs.mesh = this;
    for (const url of this.neighborUrls) { await this.addPeer(url); }
  }

  async listenLoop(baseUrl, replyTo = null, stream = null) {
      if (this.abortController.signal.aborted) return;
      try {
          const headers = { 'x-vfs-peer-id': this.vfs.id };
          if (replyTo) headers['x-vfs-reply-to'] = replyTo;
          const resp = await this.fetch(`${baseUrl}/listen`, {
              method: 'POST',
              headers,
              signal: this.abortController.signal,
              body: stream
          });

          if (resp.status === 200) {
              const command = await resp.json();
              if (command.op === 'READ') {
                  const resultStream = await this.vfs.read(command.path, command.parameters, { stack: command.stack, expiresAt: command.expiresAt });
                  return this.listenLoop(baseUrl, command.id, resultStream);
              } else if (command.op === 'SPY') {
                  const resultStream = await this.vfs.spy(command.path, command.parameters, { stack: command.stack, expiresAt: command.expiresAt });
                  return this.listenLoop(baseUrl, command.id, resultStream);
              } else if (command.op === 'SUB') {
                  this.addInterest(command.stack[0] || 'unknown', command.selector, command.expiresAt, command.stack);
                  return this.listenLoop(baseUrl);
              } else if (command.op === 'NOTIFY') {
                  this.notify(command.selector, command.payload, command.stack);
                  return this.listenLoop(baseUrl);
              }
          }
      } catch (err) {
          if (this.abortController.signal.aborted) return;
          await new Promise(resolve => {
              const timer = setTimeout(resolve, 2000);
              this.abortController.signal.addEventListener('abort', () => { clearTimeout(timer); resolve(); }, { once: true });
          });
      }
      return this.listenLoop(baseUrl);
  }

  async testReachability(url) {
    if (!url) return false;
    try {
      const resp = await this.fetch(`${url}/health`, { method: 'GET', signal: AbortSignal.timeout(2000) });
      return resp.ok;
    } catch (e) { return false; }
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
            signal: this.abortController.signal
        });
        if (resp.ok) {
            const info = await resp.json();
            const remoteId = info.id;
            if (remoteId && remoteId !== this.vfs.id) {
                if (!this.peers.has(remoteId)) {
                    const peer = new StaticPeer(remoteId, url, this.fetch, { localUrl: this.localUrl, signal: this.abortController.signal });
                    peer.reachability = 'DIRECT';
                    this.peers.set(remoteId, peer);
                    
                    // Interest Catch-up
                    for (const entry of this.interests.values()) {
                        peer.subscribe(entry.selector, Date.now() + 60000, [this.vfs.id]).catch(() => {});
                    }
                }
                if (info.reachability === 'REVERSE') this.listenLoop(url);
                return remoteId;
            }
        }
    } catch (e) {} finally { this.connecting.delete(url); }
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
          
          // Interest Catch-up
          for (const entry of this.interests.values()) {
              peer.subscribe(entry.selector, Date.now() + 60000, [this.vfs.id]).catch(() => {});
          }
      }
      const existing = this.reverseRegistry.listeners.get(peerId);
      if (existing) try { existing.writeHead(204); existing.end(); } catch(e) {}
      this.reverseRegistry.listeners.set(peerId, res);
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    if (targetPeers.length === 0) return null;
    const fetchPromises = targetPeers.map(async (peer) => {
        try { const resp = await peer.read(path, parameters, { stack: newStack, expiresAt }); if (resp) return resp; } catch (err) {}
        throw new Error('Peer failed');
    });
    try { return await Promise.any(fetchPromises); } catch (e) { return null; }
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    if (targetPeers.length === 0) return null;
    const fetchPromises = targetPeers.map(async (peer) => {
        try { return await peer.spy(path, parameters, { stack: newStack, expiresAt }); } catch (err) { return null; }
    });
    const results = await Promise.allSettled(fetchPromises);
    const validStreams = results.filter(r => r.status === 'fulfilled' && r.value).map(r => r.value);
    if (validStreams.length === 0) return null;
    return new ReadableStream({
        async start(controller) {
            for (const stream of validStreams) {
                const reader = stream.getReader();
                try { while (true) { const { done, value } = await reader.read(); if (done) break; controller.enqueue(value); } } finally { reader.releaseLock(); }
            }
            controller.close();
        }
    });
  }

  stop() {
    this.abortController.abort();
    clearInterval(this._ppsInterval);
    clearInterval(this._topoInterval);
    for (const res of this.reverseRegistry.listeners.values()) { try { res.end(); } catch(e) {} }
  }
}

export class MeshLink extends MeshLinkBase {}
