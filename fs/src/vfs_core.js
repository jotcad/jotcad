/**
 * Simple EventEmitter shim for both Node and Browser.
 */
import { 
  normalizeSelector, 
  getCID, 
  getSelectorKey, 
  encodeSafe,
  decodeSafe,
  encodeJCB, 
  decodeJCB 
} from './cid.js';

export { normalizeSelector, getCID, getSelectorKey, encodeSafe, decodeSafe, encodeJCB, decodeJCB };

export const WebReadableStream = globalThis.ReadableStream;

class EventEmitter {
  constructor() {
    this.listeners = new Map();
  }
  on(event, fn) {
    if (!this.listeners.has(event)) this.listeners.set(event, []);
    this.listeners.get(event).push(fn);
  }
  off(event, fn) {
    const l = this.listeners.get(event);
    if (l) {
      const idx = l.indexOf(fn);
      if (idx !== -1) l.splice(idx, 1);
    }
  }
  emit(event, ...args) {
    const l = this.listeners.get(event);
    if (l) l.forEach((fn) => fn(...args));
  }
}

export function isMatch(selector, target) {
  const s = normalizeSelector(selector);
  const t = normalizeSelector(target);
  if (s.path.endsWith('*')) {
    if (!t.path.startsWith(s.path.slice(0, -1))) return false;
  } else if (s.path !== t.path) return false;
  for (const [k, v] of Object.entries(s.parameters)) {
    if (JSON.stringify(v) !== JSON.stringify(t.parameters[k])) return false;
  }
  return true;
}

export class VFSClosedError extends Error {
  constructor() {
    super('VFS is closed');
    this.name = 'VFSClosedError';
  }
}

export class MemoryStorage {
  constructor(vfsId = 'unknown') {
    this.vfsId = vfsId;
    this.data = new Map();
    this.info = new Map();
  }
  async init() {}
  async has(cid) { 
    const result = this.data.has(cid);
    if (result) console.log(`[MemoryStorage ${this.vfsId}] has(${cid.slice(0, 8)}) -> TRUE`);
    return result;
  }
  async get(cid) {
    const bytes = this.data.get(cid);
    if (!bytes) {
        console.log(`[MemoryStorage ${this.vfsId}] get(${cid.slice(0, 8)}) -> MISS`);
        return null;
    }
    console.log(`[MemoryStorage ${this.vfsId}] get(${cid.slice(0, 8)}) -> HIT (${bytes.length} bytes)`);
    return new WebReadableStream({
      start(controller) { controller.enqueue(bytes); controller.close(); },
    });
  }
  async set(cid, streamOrBytes, info = {}) {
    let bytes;
    if (streamOrBytes instanceof Uint8Array) {
      bytes = streamOrBytes;
    } else if (typeof streamOrBytes === 'string') {
      bytes = new TextEncoder().encode(streamOrBytes);
    } else if (streamOrBytes === null) {
      bytes = new Uint8Array();
    } else {
      const chunks = [];
      const reader = streamOrBytes.getReader();
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
        }
      } finally { reader.releaseLock(); }
      const len = chunks.reduce((acc, c) => acc + c.length, 0);
      bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    }
    this.data.set(cid, bytes);
    this.info.set(cid, info);
    console.log(`[MemoryStorage ${this.vfsId}] set(${cid.slice(0, 8)}) - data size: ${bytes.length}, info: ${Object.keys(info)}`);
  }
  async delete(cid) { this.data.delete(cid); this.info.delete(cid); }
  async close() { this.data.clear(); this.info.clear(); }
  async *iterateMeta() {
    for (const [cid, info] of this.info.entries()) { yield { cid, info }; }
  }
}

export class VFS {
  constructor({ id, storage } = {}) {
    this.id = id || globalThis.crypto.randomUUID();
    this.storage = storage || new MemoryStorage(this.id);
    this.providers = new Map();
    this.activeWait = new Map();
    this.events = new EventEmitter();
    this.mesh = null;
    this.closed = false;
  }

  async init() {
    await this.storage.init();
  }

  _checkClosed() {
    if (this.closed) throw new VFSClosedError();
  }

  registerProvider(path, handler, options = {}) {
    this.providers.set(path, handler);
    if (options.schema) {
        this._schemas = this._schemas || new Map();
        this._schemas.set(path, options.schema);
    }
  }

  addSchema(path, schema) {
    this._schemas = this._schemas || new Map();
    this._schemas.set(path, schema);
  }

  validateSelector(s) {
    if (!this._schemas || !this._schemas.has(s.path)) return true;
    const schema = this._schemas.get(s.path);
    const params = s.parameters || {};

    if (schema.required) {
      for (const req of schema.required) {
        if (params[req] === undefined)
          throw new Error(`Missing required parameter '${req}'`);
      }
    }

    if (schema.properties) {
      for (const [key, prop] of Object.entries(schema.properties)) {
        if (params[key] !== undefined) {
          if (prop.type === 'number' && typeof params[key] !== 'number')
            throw new Error(`Invalid parameter type for '${key}': Expected number`);
          if (prop.type === 'string' && typeof params[key] !== 'string')
            throw new Error(`Invalid parameter type for '${key}': Expected string`);
          if (prop.enum && !prop.enum.includes(params[key]))
            throw new Error(`Invalid enum value for '${key}': Expected one of [${prop.enum.join(', ')}]`);
        }
      }
    }
    return true;
  }

  async getCID(data) { return getCID(data); }

  async read(selector, context = {}) {
    const s = normalizeSelector(selector);
    console.log(`[VFS] Request: ${s.path} ${JSON.stringify(s.parameters || {})}`);
    const result = await this._readResult(s, context);
    if (result && result.success) return this.storage.get(result.cid);
    if (result && result.error === 'Expired') return null;
    if (result && result.error) throw new Error(result.error);
    return null;
  }

  async write(selector, streamOrBytes, context = {}) {
    this._checkClosed();
    
    let bytes;
    let cid;

    if (streamOrBytes instanceof Uint8Array) {
        bytes = streamOrBytes;
        cid = await getCID(bytes);
    } else if (typeof streamOrBytes === 'string') {
        bytes = new TextEncoder().encode(streamOrBytes);
        cid = await getCID(streamOrBytes);
    } else if (streamOrBytes === null) {
        bytes = new Uint8Array();
        cid = await getCID(bytes);
    } else if (streamOrBytes && typeof streamOrBytes.getReader === 'function') {
        const chunks = [];
        const reader = streamOrBytes.getReader();
        try {
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
            }
        } finally { reader.releaseLock(); }
        const len = chunks.reduce((acc, c) => acc + c.length, 0);
        bytes = new Uint8Array(len);
        let offset = 0;
        for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
        cid = await getCID(bytes);
    } else {
        // Structured data: use canonical JCB for hash, but store raw JSON for readability.
        cid = await getCID(streamOrBytes);
        bytes = new TextEncoder().encode(JSON.stringify(streamOrBytes));
    }

    const s = normalizeSelector(selector);
    const addrKey = await getSelectorKey(s);

    const info = {
        cid,
        path: s.path,
        parameters: s.parameters,
        state: 'AVAILABLE',
        ...context
    };

    await this.storage.set(cid, bytes, info);
    await this.storage.set(addrKey, null, info);

    this.events.emit('state', { path: s.path, parameters: s.parameters, cid, state: 'AVAILABLE' });
    return { cid };
  }

  async writeData(selector, data, context = {}) {
    return await this.write(selector, data, context);
  }

  async readData(selector, context = {}) {
    if (!selector) throw new Error('VFS.readData: Missing required selector');
    const s = normalizeSelector(selector);
    if (!s.path && !s.parameters?.cid) throw new Error('VFS.readData: Selector must have a path or CID');

    const stream = await this.read(s, context);
    if (!stream) throw new Error(`VFS.readData: Failed to read data for selector ${JSON.stringify(s)}`);
    const chunks = [];
    const reader = stream.getReader();
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }

    console.log(`[VFS ${this.id}] readData decoded ${bytes.length} bytes`);

    // 1. Try JCB Decoding (Binary)
    try {
        const decoded = decodeJCB(bytes);
        if (decoded !== undefined) {
            console.log(`[VFS ${this.id}] readData JCB success`);
            return decoded;
        }
    } catch (e) {
        console.log(`[VFS ${this.id}] readData JCB failed: ${e.message}`);
    }

    const text = new TextDecoder().decode(bytes);
    console.log(`[VFS ${this.id}] readData text: "${text.slice(0, 50)}${text.length > 50 ? '...' : ''}"`);

    // 2. Try Safe Decoding (Base64-JCB)
    try {
        const decoded = decodeSafe(text.trim());
        if (decoded !== undefined) {
            console.log(`[VFS ${this.id}] readData Safe success`);
            return decoded;
        }
    } catch (e) {}

    // 3. Try JSON Parsing (Fallback for legacy/text providers)
    try {
        const trimmed = text.trim();
        if ((trimmed.startsWith('{') && trimmed.endsWith('}')) || (trimmed.startsWith('[') && trimmed.endsWith(']'))) {
            const parsed = JSON.parse(trimmed);
            console.log(`[VFS ${this.id}] readData JSON success`);
            return parsed;
        }
    } catch (e) {}

    return bytes;
  }

  async readText(selector, context = {}) {
    const data = await this.readData(selector, context);
    if (!data)
      throw new Error(
        `VFS.readText: Failed to read text for selector ${JSON.stringify(
          selector
        )}`
      );

    const serializeShape = async (shape) => {
      if (shape.geometry) {
        return await this.readText(shape.geometry, context);
      }
      let out = '';
      if (shape.components && Array.isArray(shape.components)) {
        for (const child of shape.components) {
          out += await serializeShape(child);
        }
      }
      return out;
    };

    if (typeof data === 'string') return data;
    if (data instanceof Uint8Array) return new TextDecoder().decode(data);
    if (typeof data === 'object' && !Array.isArray(data)) {
      // It's a Shape object
      return await serializeShape(data);
    }
    return JSON.stringify(data);
  }

  async link(src, tgt) {
    this._checkClosed();
    const s_src = normalizeSelector(src);
    const s_tgt = normalizeSelector(tgt);
    const addrKey = await getSelectorKey(s_src);
    
    await this.storage.set(addrKey, null, {
      path: s_src.path,
      parameters: s_src.parameters,
      target: s_tgt,
      state: 'LINKED'
    });
    
    this.events.emit('state', { path: s_src.path, parameters: s_src.parameters, addrKey, state: 'LINKED', target: s_tgt });
  }

  async _readResult(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 10000, followLinks = true, depth = 0 } = context;
    const s = normalizeSelector(selector);
    if (depth > 10) throw new Error(`Maximum recursion depth exceeded for ${s.path}`);
    
    // Throw validation errors immediately
    this.validateSelector(s);
    
    if (Date.now() > expiresAt) return { success: false, error: 'Expired' };

    const addrKey = await getSelectorKey(s);
    if (stack.includes(addrKey)) throw new Error(`Circular link detected for ${s.path}`);
    if (this.activeWait.has(addrKey)) return await this.activeWait.get(addrKey);

    const workPromise = (async () => {
      try {
        if (s.parameters.cid) {
            if (await this.storage.has(s.parameters.cid)) {
                console.log(`[VFS ${this.id}] Storage hit for CID ${s.parameters.cid}`);
                return { success: true, cid: s.parameters.cid };
            }
        }
        if (await this.storage.has(addrKey)) {
          const info = await this._getStorageInfo(addrKey);
          console.log(`[VFS ${this.id}] Storage hit for addrKey ${addrKey} (cid: ${info?.cid})`);
          if (followLinks && info?.target) {
            return await this._readResult(info.target, { ...context, depth: depth + 1, stack: [...stack, addrKey] });
          }
          if (info?.cid) return { success: true, cid: info.cid, metadata: info || {} };
        }

        console.log(`[VFS ${this.id}] Storage miss for ${s.path}, trying providers/mesh`);
        let provider = this.providers.get(s.path);
        if (!provider) {
          for (const [pattern, handler] of this.providers.entries()) {
            if (pattern.endsWith('*') && s.path.startsWith(pattern.slice(0, -1))) { provider = handler; break; }
          }
        }

        let resultData = null;
        if (provider) {
          resultData = await provider(this, s, { ...context, expiresAt });
        } else if (this.mesh) {
          const meshResponse = await this.mesh.read(s, { stack: [...stack, this.id], expiresAt });
          if (meshResponse) resultData = meshResponse.body || meshResponse;
        }

        if (resultData) {
          const { cid } = await this.write(s, resultData);
          return { success: true, cid };
        }
        return { success: false };
      } catch (err) {
        return { success: false, error: err.message };
      } finally {
        this.activeWait.delete(addrKey);
      }
    })();

    this.activeWait.set(addrKey, workPromise);
    return await workPromise;
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const results = [];
    const nodeId = this.id;

    // 1. Local Scan (Contributions from local storage)
    if (typeof this.storage.iterateMeta === 'function') {
      const storageResults = [];
      for await (const entry of this.storage.iterateMeta()) {
        const info = entry.info || {};
        if (info.path && (info.path === s.path || (s.path.endsWith('*') && info.path.startsWith(s.path.slice(0, -1))))) {
           // Create a VFS Bundle-style entry for this found artifact
           const payload = {
               ...info,
               metadata: info,
               provider: info.provider || nodeId
           };
           storageResults.push(new TextEncoder().encode(`\n=${JSON.stringify(payload).length} discovery\n${JSON.stringify(payload)}`));
        }
      }
      if (storageResults.length > 0) {
          results.push(new ReadableStream({
              start(controller) {
                  for (const chunk of storageResults) controller.enqueue(chunk);
                  controller.close();
              }
          }));
      }
    }

    // 2. Mesh Scan
    if (this.mesh) {
        const meshStream = await this.mesh.spy(s, context);
        if (meshStream) results.push(meshStream);
    }

    if (results.length === 0) return null;
    if (results.length === 1) return results[0];

    // Multiplex all contributors
    return new ReadableStream({
        async start(controller) {
          for (const s of results) {
            const reader = s.getReader();
            try {
              while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                controller.enqueue(value);
              }
            } catch (e) {} finally { reader.releaseLock(); }
          }
          controller.close();
        }
    });
  }

  async _getStorageInfo(key) {
    if (typeof this.storage.iterateMeta === 'function') {
      for await (const entry of this.storage.iterateMeta()) {
        if (entry.cid === key) return entry.info;
      }
    }
    return null;
  }

  async close() {
    this.closed = true;
    this.activeWait.clear();
    if (this.mesh) this.mesh.stop();
    await this.storage.close();
  }
}
