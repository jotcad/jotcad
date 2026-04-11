import crypto from 'crypto';

/**
 * MeshLink: Recursive Bread-crumb Router for the Mesh-VFS.
 */
export class MeshLinkBase {
  constructor(vfs, neighbors = [], { fetch, localUrl }) {
    this.vfs = vfs;
    this.neighbors = new Set(neighbors.map(n => this.normalizeUrl(n)));
    this.fetch = fetch;
    this.localUrl = this.normalizeUrl(localUrl);
  }

  normalizeUrl(url) {
    if (!url) return url;
    return url.replace(/\/$/, '').toLowerCase();
  }

  async start() {
    console.log(`[MeshLink ${this.vfs.id}] Started. Local: ${this.localUrl}. Neighbors: ${[...this.neighbors].join(', ')}`);
    this.vfs.mesh = this;
  }

  /**
   * Performs a Bread-crumb READ across the mesh.
   */
  async read(path, parameters, stack = []) {
    const selector = { path, parameters };
    const cid = await this.vfs.getCID(selector);
    
    const currentStack = stack.map(n => this.normalizeUrl(n));
    if (this.localUrl && !currentStack.includes(this.localUrl)) {
        currentStack.push(this.localUrl);
    }

    const targetNeighbors = [...this.neighbors].filter(n => !currentStack.includes(n));
    if (targetNeighbors.length === 0) return null;

    const fetchPromises = targetNeighbors.map(async (neighbor) => {
        try {
            const resp = await this.fetch(`${neighbor}/read`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ selector, stack: currentStack, expiresAt: Date.now() + 30000 })
            });

            if (resp.ok && resp.body) {
                // IMPORTANT: Consume the stream once and capture the bytes.
                // This ensures we can verify, store, and return it without deadlock.
                const chunks = [];
                const reader = resp.body.getReader();
                while(true) {
                    const { done, value } = await reader.read();
                    if (done) break;
                    chunks.push(value);
                }

                const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
                const data = new Uint8Array(totalLength);
                let offset = 0;
                for (const chunk of chunks) {
                    data.set(chunk, offset);
                    offset += chunk.length;
                }

                // Store locally for future hits
                await this.vfs.writeData(path, parameters, data);
                
                console.log(`[MeshLink ${this.vfs.id}] SUCCESS: ${path} from ${neighbor} (${totalLength} bytes)`);
                
                // Return a fresh stream of the captured bytes
                return new ReadableStream({
                    start(c) {
                        c.enqueue(data);
                        c.close();
                    }
                });
            }
        } catch (err) {
            console.warn(`[MeshLink ${this.vfs.id}] Neighbor ${neighbor} failed:`, err.message);
        }
        throw new Error('Neighbor failed');
    });

    try {
        return await Promise.any(fetchPromises);
    } catch (e) {
        return null;
    }
  }

  addNeighbor(url) {
    const cleanUrl = this.normalizeUrl(url);
    if (!cleanUrl || cleanUrl === this.localUrl) return;
    if (!this.neighbors.has(cleanUrl)) {
        console.log(`[MeshLink ${this.vfs.id}] LEARNING new neighbor: ${cleanUrl}`);
        this.neighbors.add(cleanUrl);
    }
  }

  stop() {}
}
