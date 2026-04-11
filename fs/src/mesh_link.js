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
            const chunks = [];
            const reader = resp.body.getReader();
            try {
                while(true) {
                    const { done, value } = await reader.read();
                    if (done) break;
                    chunks.push(value);
                }
            } finally {
                reader.releaseLock();
            }
            const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
            const data = new Uint8Array(totalLength);
            let offset = 0;
            for (const chunk of chunks) {
                data.set(chunk, offset);
                offset += chunk.length;
            }
            return data;
        }
        return null;
    }
}

/**
 * MeshLink: Peer Registry and Recursive Bread-crumb Router.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.fetch = options.fetch;
    this.localUrl = options.localUrl;
    this.peers = new Map(); // UUID -> Peer
    this.neighborUrls = neighborUrls.map(u => u.replace(/\/$/, ''));
    this.abortController = new AbortController();
  }

  async start() {
    this.vfs.mesh = this;
    await Promise.all(this.neighborUrls.map(url => this.addPeer(url)));
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

  async read(path, parameters, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const newStack = [...stack, this.vfs.id];
    const targetPeers = [...this.peers.values()].filter(p => !stack.includes(p.id));
    
    if (targetPeers.length === 0) return null;

    const selector = { path, parameters };
    const cid = await this.vfs.getCID(selector);

    const fetchPromises = targetPeers.map(async (peer) => {
        try {
            const data = await peer.read(path, parameters, { stack: newStack, expiresAt });
            if (data) {
                await this.vfs.writeData(path, parameters, data);
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
  }
}
