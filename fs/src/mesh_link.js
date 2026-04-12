import crypto from 'crypto';

/**
 * Peer: Abstract interface for any entity we can request data from.
 */
class Peer {
    constructor(id) {
        this.id = id;
    }
    async read(path, parameters, stack) {
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
            'x-vfs-id': stack[0] // Originator hint
        };
        if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

        const resp = await this.fetch(`${this.url}/read`, {
            method: 'POST',
            headers,
            signal: this.signal,
            body: JSON.stringify({ 
                path, 
                parameters, 
                stack,
                expiresAt 
            })
        });

        if (resp.ok && resp.body) {
            return resp.body;
        }
        return null;
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

            this.registry.replies.set(requestId, (stream) => {
                clearTimeout(timeout);
                resolve(stream);
            });
        });

        const dispatched = this.registry.dispatch(this.id, {
            type: 'COMMAND',
            op: 'READ',
            id: requestId,
            path,
            parameters,
            stack,
            expiresAt
        });

        if (!dispatched) {
            this.registry.replies.delete(requestId);
            return null;
        }

        return replyPromise;
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
    this.peers = new Map(); // UUID -> Peer
    this.neighborUrls = neighborUrls.map(u => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.reverseRegistry = {
        listeners: new Map(), // PeerID -> Response Object
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
  }

  async start() {
    this.vfs.mesh = this;
    await Promise.all(this.neighborUrls.map(url => this.addPeer(url)));
    
    if (this.neighborUrls.length > 0) {
        this.listenLoop(this.neighborUrls[0]);
    }
  }

  async listenLoop(baseUrl, replyTo = null, stream = null) {
      if (this.abortController.signal.aborted) return;

      try {
          const resp = await this.fetch(`${baseUrl}/listen`, {
              method: 'POST',
              headers: {
                  'x-vfs-peer-id': this.vfs.id,
                  'x-vfs-reply-to': replyTo || ''
              },
              body: stream,
              duplex: 'half',
              signal: this.abortController.signal
          });

          if (resp.status === 200) {
              const text = await resp.text();
              if (text) {
                  const command = JSON.parse(text);
                  
                  if (command.type === 'COMMAND' && command.op === 'READ') {
                      // As soon as we receive one /listen response we can open another /listen request
                      this.listenLoop(baseUrl); 

                      const resultStream = await this.vfs.read(command.path, command.parameters, { 
                          stack: command.stack,
                          expiresAt: command.expiresAt 
                      });
                      
                      // Send the reply in a dedicated poll
                      return this.listenLoop(baseUrl, command.id, resultStream);
                  }
              }
          }
      } catch (err) {
          if (err.name === 'AbortError') return;
          console.warn(`[MeshLink ${this.vfs.id}] Listen loop error:`, err);
          await new Promise(r => setTimeout(r, 1000));
      }
      
      return this.listenLoop(baseUrl);
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    try {
        const resp = await this.fetch(`${url}/health`, { signal: this.abortController.signal });
        if (resp.ok) {
            const info = await resp.json();
            if (info.id && info.id !== this.vfs.id) {
                if (!this.peers.has(info.id)) {
                    this.peers.set(info.id, new StaticPeer(info.id, url, this.fetch, { 
                        localUrl: this.localUrl,
                        signal: this.abortController.signal 
                    }));
                }
                return info.id;
            }
        }
    } catch (err) {}
    return null;
  }

  registerReversePeer(peerId, res, replyTo = null, stream = null) {
      if (replyTo && stream) {
          const resolve = this.reverseRegistry.replies.get(replyTo);
          if (resolve) {
              this.reverseRegistry.replies.delete(replyTo);
              resolve(stream);
          }
      }

      if (!this.peers.has(peerId)) {
          this.peers.set(peerId, new ReversePeer(peerId, this.reverseRegistry));
      }
      
      const existing = this.reverseRegistry.listeners.get(peerId);
      if (existing) {
          try {
            existing.writeHead(204);
            existing.end();
          } catch(e) {}
      }

      this.reverseRegistry.listeners.set(peerId, res);
  }

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    
    if (targetPeers.length === 0) return null;

    const selector = { path, parameters };
    const cid = await this.vfs.getCID(selector);

    const fetchPromises = targetPeers.map(async (peer) => {
        try {
            const stream = await peer.read(path, parameters, { stack: newStack, expiresAt });
            if (stream) {
                await this.vfs.write(path, parameters, stream);
                return true;
            }
        } catch (err) {}
        throw new Error('Peer failed');
    });

    try {
        return await Promise.any(fetchPromises);
    } catch (e) {
        return false;
    }
  }

  stop() {
    this.abortController.abort();
    for (const res of this.reverseRegistry.listeners.values()) {
        try { res.end(); } catch(e) {}
    }
  }
}

/**
 * MeshLink: Concrete environment-agnostic MeshLink implementation.
 */
export class MeshLink extends MeshLinkBase {}
