/**
 * Simple EventEmitter shim for both Node and Browser.
 */
import { 
  normalizeSelector, 
  getCID, 
  getSelectorKey, 
  encodeJCB, 
  decodeJCB 
} from './cid.js';

export { normalizeSelector, getCID, getSelectorKey, encodeJCB, decodeJCB };

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
  constructor() {
    this.data = new Map();
    this.info = new Map();
  }
  async init() {}
  async has(cid) { return this.data.has(cid); }
  async get(cid) {
    const bytes = this.data.get(cid);
    if (!bytes) return null;
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
  }
  async delete(cid) { this.data.delete(cid); this.info.delete(cid); }
  async close() { this.data.clear(); this.info.clear(); }
  async *iterateMeta() {
    for (const [cid, info] of this.info.entries()) { yield { cid, info }; }
  }
}

export class VFS {
  constructor({ id, storage = new MemoryStorage() } = {}) {
    this.id = id || globalThis.crypto.randomUUID();
    this.storage = storage;
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
    const result = await this._readResult(s, context);
    if (result && result.success) return this.storage.get(result.cid);
    if (result && result.error === 'Expired') return null;
    if (result && result.error) throw new Error(result.error);
    return null;
  }

  async write(selector, streamOrBytes, info = {}) {
    this._checkClosed();
    
    let bytes;
    if (streamOrBytes instanceof Uint8Array) {
        bytes = streamOrBytes;
    } else if (typeof streamOrBytes === 'string') {
        bytes = new TextEncoder().encode(streamOrBytes);
    } else if (streamOrBytes === null) {
        bytes = new Uint8Array();
    } else if (typeof streamOrBytes.getReader === 'function') {
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
    } else {
        bytes = new TextEncoder().encode(encodeSafe(streamOrBytes));
    }

    const cid = await getCID(bytes);
    await this.storage.set(cid, bytes);

    const s = normalizeSelector(selector);
    const addrKey = await getSelectorKey(s);
    
    await this.storage.set(addrKey, null, {
        ...info,
        cid,
        path: s.path,
        parameters: s.parameters,
        state: 'AVAILABLE'
    });

    this.events.emit('state', { path: s.path, parameters: s.parameters, cid, state: 'AVAILABLE' });
    return { cid };
  }

  async writeData(selector, data) {
    return await this.write(selector, data);
  }

  async readData(selector, context = {}) {
    const s = normalizeSelector(selector);
    const stream = await this.read(s, context);
    if (!stream) return null;
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
    
    // 1. Try JCB Decoding (Binary)
    try {
        return decodeJCB(bytes);
    } catch (e) {}

    const text = new TextDecoder().decode(bytes);

    // 2. Try Safe Decoding (Base64-JCB)
    try {
        return decodeSafe(text.trim());
    } catch (e) {}

    // 3. Try JSON Parsing (Fallback for legacy/text providers)
    try {
        const trimmed = text.trim();
        if ((trimmed.startsWith('{') && trimmed.endsWith('}')) || (trimmed.startsWith('[') && trimmed.endsWith(']'))) {
            return JSON.parse(trimmed);
        }
    } catch (e) {}
    
    return bytes;
  }

  async readText(selector, context = {}) {
    const data = await this.readData(selector, context);
    if (!data) return null;
    if (typeof data === 'string') return data;
    if (data instanceof Uint8Array) return new TextDecoder().decode(data);
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
            if (await this.storage.has(s.parameters.cid)) return { success: true, cid: s.parameters.cid };
        }
        if (await this.storage.has(addrKey)) {
          const info = await this._getStorageInfo(addrKey);
          if (followLinks && info?.target) {
            return await this._readResult(info.target, { ...context, depth: depth + 1, stack: [...stack, addrKey] });
          }
          if (info?.cid) return { success: true, cid: info.cid, metadata: info || {} };
        }

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
    if (this.mesh) return await this.mesh.spy(s, context);
    return null;
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
