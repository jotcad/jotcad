/**
 * Simple EventEmitter shim for both Node and Browser.
 */
import crypto from 'crypto';

export const WebReadableStream = globalThis.ReadableStream;

class EventEmitter {
  constructor() { this.listeners = new Map(); }
  on(event, cb) {
    if (!this.listeners.has(event)) this.listeners.set(event, new Set());
    this.listeners.get(event).add(cb);
  }
  off(event, cb) { this.listeners.get(event)?.delete(cb); }
  emit(event, data) { this.listeners.get(event)?.forEach(cb => cb(data)); }
}

/**
 * Deterministic Selector Normalization.
 */
export const normalizeSelector = (pathOrSelector, params = {}) => {
  if (typeof pathOrSelector === 'object' && pathOrSelector !== null) {
    return normalizeSelector(pathOrSelector.path, pathOrSelector.parameters);
  }
  const path = pathOrSelector;

  const clean = (obj) => {
    if (!obj || typeof obj !== 'object') return obj;
    if (Array.isArray(obj)) return obj.map(clean);
    const result = {};
    const keys = Object.keys(obj).sort();
    for (const k of keys) result[k] = clean(obj[k]);
    return result;
  };

  const result = { path, parameters: clean(params) };
  const sortedResult = {};
  Object.keys(result).sort().forEach(k => sortedResult[k] = result[k]);
  return sortedResult;
};

export const isMatch = (s1, s2) => {
  if (!s1 || !s2) return s1 === s2;
  const a = normalizeSelector(s1);
  const b = normalizeSelector(s2);
  return JSON.stringify(a) === JSON.stringify(b);
};

export class VFSClosedError extends Error {
  constructor() { super('VFS is closed'); this.name = 'VFSClosedError'; }
}

export class MemoryStorage {
  constructor() {
    this.results = new Map();
  }
  async get(cid) {
    const entry = this.results.get(cid);
    if (!entry) return null;
    const data = entry.data;
    return new WebReadableStream({
      start(c) {
        if (data.length > 0) c.enqueue(data);
        c.close();
      },
    });
  }
  async set(cid, stream, info) {
    let finalData;
    if (stream instanceof Uint8Array) {
        finalData = stream;
    } else if (typeof stream === 'string') {
        finalData = new TextEncoder().encode(stream);
    } else {
        const chunks = [];
        if (typeof stream?.getReader === 'function') {
            const reader = stream.getReader();
            try {
                while (true) {
                    const { done, value } = await reader.read();
                    if (done) break;
                    chunks.push(value);
                }
            } finally {
                reader.releaseLock();
            }
        } else if (stream?.[Symbol.asyncIterator]) {
            for await (const chunk of stream) chunks.push(chunk);
        } else {
            return;
        }
        
        const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
        finalData = new Uint8Array(totalLength);
        let offset = 0;
        for (const chunk of chunks) {
            finalData.set(chunk, offset);
            offset += chunk.length;
        }
    }
    this.results.set(cid, { data: finalData, info });
  }
  async has(cid) {
    return this.results.has(cid);
  }
  async delete(cid) {
    this.results.delete(cid);
  }
  async close() {
    this.results.clear();
  }
}

export class VFS {
  constructor(options = {}) {
    this.id = options.id || crypto.randomUUID();
    this.storage = options.storage || new MemoryStorage();
    this.getCID = options.getCID || (async (s) => crypto.createHash('sha256').update(JSON.stringify(s)).digest('hex'));
    this.providers = new Map();
    this.activeWait = new Map(); // SelectorKey -> Promise<{success: boolean, cid: string}>
    this.events = new EventEmitter();
    this.mesh = null;
    this.closed = false;
  }

  registerProvider(path, handler) {
    this.providers.set(path, handler);
  }

  async localRead(pathOrSelector, parameters, context = {}) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    
    if (await this.storage.has(cid)) return this.storage.get(cid);

    const provider = this.providers.get(s.path);
    if (provider) {
        // If there's a provider, read() will handle the deduplication
        return null; 
    }
    return null;
  }

  async read(pathOrSelector, parameters, context = {}) {
    const s = normalizeSelector(pathOrSelector, parameters);
    const key = JSON.stringify(s);
    const { stack = [], expiresAt = Date.now() + 30000 } = context;

    if (this.activeWait.has(key)) {
        const { success, cid } = await this.activeWait.get(key);
        return success ? this.storage.get(cid) : null;
    }

    const workPromise = (async () => {
        try {
            const cid = await this.getCID(s);
            
            // 1. Try local storage
            if (await this.storage.has(cid)) return { success: true, cid };

            // 2. Try local provider
            let provider = this.providers.get(s.path);
            if (!provider) {
                // Try suffix/prefix matches
                for (const [pattern, handler] of this.providers.entries()) {
                    if (pattern.startsWith('.') && s.path.endsWith(pattern)) {
                        provider = handler;
                        break;
                    }
                    if (pattern.endsWith('/') && s.path.startsWith(pattern)) {
                        provider = handler;
                        break;
                    }
                }
            }

            if (provider) {
                const stream = await provider(this, s, { ...context, expiresAt });
                if (stream) {
                    await this.write(s.path, s.parameters, stream);
                    return { success: true, cid };
                }
            }

            // 3. Try mesh
            if (this.mesh) {
                const success = await this.mesh.read(s.path, s.parameters, { stack, expiresAt });
                return { success, cid };
            }

            return { success: false, cid };
        } catch (err) {
            console.error(`[VFS ${this.id}] Read error:`, err);
            return { success: false, cid: '' };
        } finally {
            this.activeWait.delete(key);
        }
    })();

    this.activeWait.set(key, workPromise);
    const { success, cid } = await workPromise;
    return success ? this.storage.get(cid) : null;
  }

  async readData(pathOrSelector, parameters, context = {}) {
    const stream = await this.read(pathOrSelector, parameters, context);
    if (!stream) return null;
    
    if (typeof stream.getReader !== 'function') {
        throw new Error(`Invalid stream returned from read: ${typeof stream}`);
    }

    const chunks = [];
    const reader = stream.getReader();
    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
        }
    } finally {
        reader.releaseLock();
    }
    
    let bytes;
    if (typeof Buffer !== 'undefined') {
      bytes = Buffer.concat(chunks.map(c => Buffer.from(c)));
    } else {
      let len = chunks.reduce((acc, c) => acc + c.length, 0);
      bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.length;
      }
    }
    
    const text = new TextDecoder().decode(bytes);
    try {
        const trimmed = text.trim();
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) {
            return JSON.parse(trimmed);
        }
    } catch(e) {}
    
    return bytes;
  }

  async readText(pathOrSelector, parameters, context = {}) {
    const stream = await this.read(pathOrSelector, parameters, context);
    if (!stream) return null;
    
    const chunks = [];
    const reader = stream.getReader();
    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
        }
    } finally {
        reader.releaseLock();
    }
    
    let bytes;
    if (typeof Buffer !== 'undefined') {
      bytes = Buffer.concat(chunks.map(c => Buffer.from(c)));
    } else {
      let len = chunks.reduce((acc, c) => acc + c.length, 0);
      bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.length;
      }
    }
    
    return new TextDecoder().decode(bytes);
  }

  async write(pathOrSelector, parameters, stream) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    await this.storage.set(cid, stream, { path: s.path, parameters: s.parameters });
    this.events.emit('state', { path: s.path, parameters: s.parameters, cid, state: 'AVAILABLE' });
  }

  async writeData(pathOrSelector, parameters, data) {
    let bytes;
    if (data instanceof Uint8Array) bytes = data;
    else if (typeof data === 'string') bytes = new TextEncoder().encode(data);
    else bytes = new TextEncoder().encode(JSON.stringify(data, null, 2));

    const stream = new WebReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      },
    });
    await this.write(pathOrSelector, parameters, stream);
  }

  /**
   * Injects a state event into the local VFS listeners.
   * Does not perform mesh-wide broadcast.
   */
  async receive(event) {
    this.events.emit('state', event);
  }

  async close() {
    this.closed = true;
    this.activeWait.clear();
    if (this.mesh) this.mesh.stop();
    await this.storage.close();
  }

  _checkClosed() { if (this.closed) throw new VFSClosedError(); }
}
