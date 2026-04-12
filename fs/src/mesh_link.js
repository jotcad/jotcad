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
    async spy(path, parameters, stack) {
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
            return {
                body: resp.body,
                headers: resp.headers
            };
        }
        return null;
    }

    async spy(path, parameters, context = {}) {
        const { stack = [], expiresAt = Date.now() + 30000 } = context;
        const headers = { 
            'Content-Type': 'application/json',
            'x-vfs-id': stack[0]
        };
        if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

        const resp = await this.fetch(`${this.url}/spy`, {
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

            this.registry.replies.set(requestId, (stream, headers = new Map()) => {
                clearTimeout(timeout);
                resolve({
                    body: stream,
                    headers: headers
                });
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
                resolve({
                    body: stream,
                    headers: headers
                });
            });
        });

        const dispatched = this.registry.dispatch(this.id, {
            type: 'COMMAND',
            op: 'SPY',
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
    this.connecting = new Set(); // URL -> Boolean
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
    for (const url of this.neighborUrls) {
      await this.addPeer(url);
    }
  }

  async listenLoop(baseUrl, replyTo = null, stream = null) {
      console.log(`[MeshLink ${this.vfs.id}] listenLoop (replyTo: ${replyTo || "none"})`);
      if (this.abortController.signal.aborted) return;

      try {
          // If we have a result stream to send, send it and wait for the NEXT command
          const headers = {
              'x-vfs-peer-id': this.vfs.id,
              'x-vfs-reply-to': replyTo || ''
          };

          const resp = await this.fetch(`${baseUrl}/listen`, {
              method: 'POST',
              headers,
              body: stream,
              duplex: 'half',
              signal: this.abortController.signal
          });

          console.log(`[MeshLink ${this.vfs.id}] Poll to ${baseUrl}/listen returned status: ${resp.status}`);
          
          const text = await resp.text();
          console.log(`[MeshLink ${this.vfs.id}] Poll response text: '${text}'`);

          if (resp.status === 200) {
              if (text && text.trim() !== "") {
                  const command = JSON.parse(text); console.log(`[MeshLink ${this.vfs.id}] Received command:`, command.op, "ID:", command.id);
                  
                  if (command.type === 'COMMAND') {
                      let resultStream = null;
                      if (command.op === 'READ') {
                          resultStream = await this.vfs.read(command.path, command.parameters, { 
                              stack: command.stack,
                              expiresAt: command.expiresAt 
                          });
                      } else if (command.op === 'SPY') {
                          resultStream = await this.vfs.spy(command.path, command.parameters, { 
                              stack: command.stack,
                              expiresAt: command.expiresAt 
                          });
                      }
                      
                      // Continue loop with the new command's result
                      return this.listenLoop(baseUrl, command.id, resultStream);
                  }
              }
          }
      } catch (err) {
          if (err.name === 'AbortError' || this.abortController.signal.aborted) return;
          console.warn(`[MeshLink ${this.vfs.id}] Listen loop error:`, err);
          
          // Use a cancellable promise for backoff
          await new Promise(resolve => {
              const timer = setTimeout(resolve, 2000);
              this.abortController.signal.addEventListener('abort', () => {
                  clearTimeout(timer);
                  resolve();
              }, { once: true });
          });
      }
      
      // Always maintain an idle connection
      return this.listenLoop(baseUrl);
  }

  async testReachability(url) {
    if (!url) return false;
    try {
      const resp = await this.fetch(`${url}/health`, { 
          method: 'GET',
          signal: AbortSignal.timeout(2000) 
      });
      return resp.ok;
    } catch (e) {
      return false;
    }
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    if (this.connecting.has(url)) return;
    this.connecting.add(url);

    try {
        console.log(`[MeshLink ${this.vfs.id}] Handshaking with ${url}...`);
        const resp = await this.fetch(`${url}/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                id: this.vfs.id, 
                url: this.localUrl 
            }),
            signal: this.abortController.signal
        });

        if (resp.ok) {
            const info = await resp.json();
            const remoteId = info.id;
            if (remoteId && remoteId !== this.vfs.id) {
                if (!this.peers.has(remoteId)) {
                    this.peers.set(remoteId, new StaticPeer(remoteId, url, this.fetch, { 
                        localUrl: this.localUrl,
                        signal: this.abortController.signal 
                    }));
                }
                
                if (info.reachability === 'REVERSE') {
                    console.log(`[MeshLink ${this.vfs.id}] Reverse linkage needed for ${url}`);
                    this.listenLoop(url);
                } else {
                    console.log(`[MeshLink ${this.vfs.id}] Direct linkage confirmed for ${url}`);
                }
                return remoteId;
            }
        } else {
            // Legacy fallback
            const hResp = await this.fetch(`${url}/health`, { signal: this.abortController.signal });
            if (hResp.ok) {
                const info = await hResp.json();
                this.peers.set(info.id, new StaticPeer(info.id, url, this.fetch, { localUrl: this.localUrl }));
                this.listenLoop(url);
                return info.id;
            }
        }
    } catch (err) {
        console.warn(`[MeshLink ${this.vfs.id}] Handshake failed for ${url}:`, err.message);
    } finally {
        this.connecting.delete(url);
    }
    return null;
  }

  registerReversePeer(peerId, res, replyTo = null, stream = null) {
      if (replyTo && stream) {
          const resolve = this.reverseRegistry.replies.get(replyTo);
          if (resolve) {
              this.reverseRegistry.replies.delete(replyTo);
              
              // Try to extract length if this was a chunked or pre-sized stream
              const headers = new Map();
              if (stream.length) headers.set('Content-Length', stream.length.toString());
              
              resolve(stream, headers);
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
    }
  }

  async spy(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    
    if (targetPeers.length === 0) return null;

    const fetchPromises = targetPeers.map(async (peer) => {
        try {
            // Explicitly catch peer errors so one crash doesn't kill the mesh request
            return await peer.spy(path, parameters, { stack: newStack, expiresAt });
        } catch (err) {
            console.warn(`[MeshLink ${this.vfs.id}] Spy peer error (peer: ${peer.id}):`, err);
            return null;
        }
    });

    const results = await Promise.allSettled(fetchPromises);
    const streams = results
        .filter(r => r.status === 'fulfilled' && r.value)
        .map(r => r.value);

    if (streams.length === 0) return null;

    return new (globalThis.ReadableStream)({
        async start(controller) {
            for (const stream of streams) {
                if (typeof stream.getReader === 'function') {
                    const reader = stream.getReader();
                    try {
                        while (true) {
                            const { done, value } = await reader.read();
                            if (done) break;
                            controller.enqueue(value);
                        }
                    } catch (e) {} finally {
                        reader.releaseLock();
                    }
                } else if (stream instanceof Uint8Array) {
                    controller.enqueue(stream);
                }
            }
            controller.close();
        }
    });
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
