import { EventEmitter } from './event_emitter.js';
import { MemoryStorage } from './memory_storage.js';
import { 
  normalizeSelector, 
  getCID, 
  getSelectorKey, 
  encodeSafe, 
  decodeSafe, 
  encodeJCB, 
  decodeJCB 
} from './cid.js';

export class VFSClosedError extends Error {
  constructor() { super('VFS is closed'); this.name = 'VFSClosedError'; }
}

export class VFS {
  constructor({ id, storage } = {}) {
    this.id = id || 'node';
    this.storage = storage || new MemoryStorage(this.id);
    this.providers = new Map();
    this.events = new EventEmitter();
    this.mesh = null;
    this.closed = false;
    this.activeWait = new Map();
    this.schemas = new Map();
  }

  async init() { await this.storage.init(); }

  _checkClosed() { if (this.closed) throw new VFSClosedError(); }

  registerProvider(pattern, handler, options = {}) { 
    this.providers.set(pattern, handler); 
    if (options.schema) {
        this.addSchema(pattern, options.schema);
    }
  }

  _matches(s, t) {
    if (!s || !t) return false;
    if (s.path.endsWith('*')) {
      if (!t.path.startsWith(s.path.slice(0, -1))) return false;
    } else if (s.path !== t.path) return false;
    for (const [k, v] of Object.entries(s.parameters)) {
      if (JSON.stringify(v) !== JSON.stringify(t.parameters[k])) return false;
    }
    return true;
  }

  addSchema(path, schema) {
    this.schemas.set(path, schema);
  }

  validateSelector(selector) {
    if (!selector || typeof selector !== 'object') throw new Error('Invalid selector');
    if (typeof selector.path !== 'string') throw new Error('Selector path must be a string');
    
    const schema = this.schemas.get(selector.path);
    if (schema) {
      const params = selector.parameters || {};
      if (schema.required) {
        for (const req of schema.required) {
            if (params[req] === undefined) {
                throw new Error(`Missing required parameter '${req}'`);
            }
        }
      }
      if (schema.properties) {
        for (const [name, def] of Object.entries(schema.properties)) {
          if (params[name] !== undefined) {
            if (def.type === 'number' && typeof params[name] !== 'number') {
              throw new Error(`Invalid parameter type for '${name}'`);
            }
            if (def.enum && !def.enum.includes(params[name])) {
              throw new Error(`Invalid enum value for '${name}'`);
            }
          }
        }
      }
    }
  }

  async getCID(data) { return getCID(data); }

  async read(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { expiresAt = Date.now() + 10000 } = context;
    
    // VALIDATE EARLY - this throws if invalid
    try {
        this.validateSelector(s);
    } catch (err) {
        throw err;
    }
    
    if (Date.now() > expiresAt) {
        console.log(`[VFS] read() already EXPIRED for ${s.path}`);
        return null;
    }

    // New packet: clean stack, inherit deadline
    const packetContext = { ...context, stack: [], expiresAt };

    if (context.followLinks === false) {
        const addrKey = await getSelectorKey(s);
        if (await this.storage.has(addrKey)) {
            return this.storage.get(addrKey);
        }
        return null;
    }

    const result = await this._readResult(s, packetContext);
    if (result && result.success) return this.storage.get(result.cid);
    
    if (result && result.error && !['Expired', 'Backflow'].includes(result.error)) {
        throw new Error(result.error);
    }
    return null;
  }

  async write(selector, streamOrBytes, context = {}) {
    this._checkClosed();
    let bytes, cid, type = 'bytes';

    if (streamOrBytes instanceof Uint8Array) {
        bytes = streamOrBytes;
        cid = await getCID(bytes);
    } else if (typeof streamOrBytes === 'string') {
        bytes = new TextEncoder().encode(streamOrBytes);
        cid = await getCID(streamOrBytes);
        type = 'string';
    } else if (streamOrBytes === null) {
        bytes = new Uint8Array();
        cid = await getCID(bytes);
        type = 'null';
    } else if (streamOrBytes && typeof streamOrBytes.getReader === 'function') {
        const chunks = [];
        const reader = streamOrBytes.getReader();
        try {
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                if (value) chunks.push(value);
            }
        } finally { reader.releaseLock(); }
        const len = chunks.reduce((acc, c) => acc + c.length, 0);
        bytes = new Uint8Array(len);
        let offset = 0;
        for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
        cid = await getCID(bytes);
    } else {
        cid = await getCID(streamOrBytes);
        bytes = new TextEncoder().encode(JSON.stringify(streamOrBytes));
        type = 'json';
    }

    const s = normalizeSelector(selector);
    const addrKey = await getSelectorKey(s);

    const info = {
        type,
        ...context,
        path: s.path,
        parameters: s.parameters,
        state: 'AVAILABLE',
        cid
    };
    if (type === 'json' && info.type !== 'bytes') info.type = 'json';
    delete info.expiresAt;

    if (type === 'json' || type === 'string') info.data = streamOrBytes;

    // Extract tags from original data if it's a Shape
    if (streamOrBytes && typeof streamOrBytes === 'object' && streamOrBytes.tags) {
        info.tags = { ...info.tags, ...streamOrBytes.tags };
    }
// Use consistently computed 'bytes' (drained from stream) for storage
await this.storage.set(cid, bytes, info);

// Store by addressKey (Mesh key) with Partitioned Ports
const port = context.output || '$out';
let addrInfo = { ...info, ports: {} };
if (await this.storage.has(addrKey)) {
    const existing = await this._getStorageInfo(addrKey);
    if (existing && existing.ports) addrInfo.ports = { ...existing.ports };
}
addrInfo.ports[port] = cid;
addrInfo.cid = addrInfo.ports['$out'] || cid;

await this.storage.set(addrKey, null, addrInfo);

this.events.emit('state', { path: s.path, parameters: s.parameters, cid, port, state: 'AVAILABLE' });
return { cid };
}

  async writeData(selector, data, context = {}) { return await this.write(selector, data, context); }

  async readData(selector, context = {}) {
    if (!selector) throw new Error('VFS.readData: Missing required selector');
    const s = normalizeSelector(selector);
    const { expiresAt = Date.now() + 10000 } = context;

    const result = await this._readResult(s, { ...context, stack: [], expiresAt });
    if (!result || !result.success) {
        if (result?.error === 'Expired' || result?.error === 'Backflow' || result?.error === 'Missing Port') return null;
        throw new Error(`VFS.readData: Failed to resolve selector ${JSON.stringify(s)}`);
    }
    
    const stream = await this.storage.get(result.cid);
    if (!stream) throw new Error(`VFS.readData: Failed to read stream for CID ${result.cid}`);

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
    const meta = result.metadata || {};
    if (meta.type === 'null') return null;
    
    let parsed;
    if (meta.type === 'json' && meta.data && typeof meta.data === 'object') {
        parsed = meta.data;
    } else if (meta.type === 'string' && typeof meta.data === 'string') {
        return meta.data;
    } else {
        const text = new TextDecoder().decode(bytes);
        const trimmed = text.trim();
        try {
            const decoded = decodeSafe(trimmed);
            if (decoded !== undefined) parsed = decoded;
        } catch (e) {}
        
        if (parsed === undefined) {
            try {
                if ((trimmed.startsWith('{') && trimmed.endsWith('}')) || (trimmed.startsWith('[') && trimmed.endsWith(']')) || (trimmed.startsWith('"') && trimmed.endsWith('"'))) {
                    parsed = JSON.parse(trimmed);
                }
            } catch (e) {}
        }
    }

    if (parsed && typeof parsed === 'object' && !Array.isArray(parsed)) {
        // Restore tags from metadata (content identity)
        if (meta.tags) {
            parsed.tags = { ...meta.tags, ...parsed.tags };
        }
        return parsed;
    }

    if (meta.type === 'string') return new TextDecoder().decode(bytes);
    return parsed || bytes;
  }

  async readText(selector, context = {}) {
    const data = await this.readData(selector, context);
    if (data === null || data === undefined) return null;
    const serializeShape = async (shape) => {
      if (shape.geometry) return await this.readText(shape.geometry, context);
      let out = '';
      if (shape.components && Array.isArray(shape.components)) {
        for (const child of shape.components) out += await serializeShape(child);
      }
      return out;
    };
    if (typeof data === 'string') return data;
    if (data instanceof Uint8Array) return new TextDecoder().decode(data);
    if (typeof data === 'object' && !Array.isArray(data)) return await serializeShape(data);
    return JSON.stringify(data);
  }

  async link(src, tgt) {
    this._checkClosed();
    const s_src = normalizeSelector(src);
    const s_tgt = normalizeSelector(tgt);
    const addrKey = await getSelectorKey(s_src);
    const info = { path: s_src.path, parameters: s_src.parameters, target: s_tgt, state: 'LINKED' };
    const linkText = `vfs:/${s_tgt.path}${Object.keys(s_tgt.parameters).length ? '?' + JSON.stringify(s_tgt.parameters) : ''}`;
    await this.storage.set(addrKey, new TextEncoder().encode(linkText), info);
    this.events.emit('state', { path: s_src.path, parameters: s_src.parameters, addrKey, state: 'LINKED', target: s_tgt });
  }

  async _readResult(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 10000, followLinks = true, depth = 0 } = context;
    const s = normalizeSelector(selector);
    if (depth > 20) throw new Error(`Maximum recursion depth exceeded for ${s.path}`);
    this.validateSelector(s);
    if (Date.now() > expiresAt) return { success: false, error: 'Expired' };

    const addrKey = await getSelectorKey(s);
    if (stack.includes(addrKey)) throw new Error(`Circular link detected for ${s.path}`);
    if (this.activeWait.has(addrKey)) return await this.activeWait.get(addrKey);

    const workPromise = (async () => {
      try {
        if (s.parameters.cid) {
            if (await this.storage.has(s.parameters.cid)) {
                const info = await this._getStorageInfo(s.parameters.cid);
                return { success: true, cid: s.parameters.cid, metadata: info || {} };
            }
        }
        if (await this.storage.has(addrKey)) {
          const info = await this._getStorageInfo(addrKey);
          if (followLinks && info?.target) return await this._readResult(info.target, { ...context, depth: depth + 1, stack: [...stack, addrKey] });
          
          const port = context.output || '$out';
          if (info?.ports && info.ports[port]) {
              const cidInfo = await this._getStorageInfo(info.ports[port]);
              return { success: true, cid: info.ports[port], metadata: cidInfo || info || {} };
          }
          if (info?.ports && port !== '$out' && !info.ports[port]) {
              return { success: false, error: 'Missing Port' };
          }

          if (info?.cid || info?.target) {
              const cidInfo = info.cid ? await this._getStorageInfo(info.cid) : null;
              return { success: true, cid: info.cid || addrKey, metadata: cidInfo || info || {} };
          }
        }

        // Remote packet logic
        const isBackflow = stack.includes(this.id);
        const nextStack = [...stack, this.id, addrKey];

        let provider = this.providers.get(s.path);
        if (!provider) {
          for (const [pattern, handler] of this.providers.entries()) {
            if (pattern.endsWith('*') && s.path.startsWith(pattern.slice(0, -1))) { provider = handler; break; }
          }
        }

        let resultData = null, meshInfo = {};
        if (provider) {
          // Provider execution: independent packet
          resultData = await provider(this, s, { ...context, expiresAt });
        } else if (this.mesh && !isBackflow) {
          const meshResponse = await this.mesh.read(s, { stack: nextStack, expiresAt });
          if (meshResponse) {
              resultData = meshResponse.body || meshResponse;
              if (meshResponse.headers) {
                  const infoHeader = meshResponse.headers.get('x-vfs-info');
                  if (infoHeader) { try { meshInfo = decodeSafe(infoHeader); } catch (e) {} }
              }
          }
        } else if (isBackflow) {
            return { success: false, error: 'Backflow' };
        }

        if (resultData) {
          const { cid } = await this.write(s, resultData, meshInfo);
          const meta = await this._getStorageInfo(cid);
          return { success: true, cid, metadata: meta || {} };
        }
        return { success: false };
      } catch (err) {
        return { success: false, error: err.message };
      } finally { this.activeWait.delete(addrKey); }
    })();

    this.activeWait.set(addrKey, workPromise);
    return await workPromise;
  }

  async _readResultDirect(selector, context = {}) {
      return await this._readResult(selector, { ...context, followLinks: false });
  }

  async _getStorageInfo(key) {
    if (typeof this.storage.iterateMeta === 'function') {
      for await (const entry of this.storage.iterateMeta()) {
        if (entry.cid === key || entry.info?.cid === key) return entry.info;
      }
    }
    return null;
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    if (stack.includes(this.id)) return null;
    const nextStack = [...stack, this.id];

    const streams = [];
    const localMatches = [];
    if (typeof this.storage.iterateMeta === 'function') {
        for await (const entry of this.storage.iterateMeta()) {
            if (this._matches(s, entry.info)) {
                const bundleEntry = `\n=${JSON.stringify(entry.info).length} ${entry.cid}\n${JSON.stringify(entry.info)}`;
                localMatches.push(new TextEncoder().encode(bundleEntry));
            }
        }
    }
    if (localMatches.length > 0) {
        streams.push(new ReadableStream({
            start(controller) {
                for (const m of localMatches) controller.enqueue(m);
                controller.close();
            }
        }));
    }

    if (this.mesh) {
        const meshStream = await this.mesh.spy(s, { stack: nextStack, expiresAt });
        if (meshStream) streams.push(meshStream);
    }

    if (streams.length === 0) return null;
    return new ReadableStream({
      async start(controller) {
        for (const s of streams) {
          const reader = s.getReader();
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
  }

  async close() {
    this.closed = true;
    this.activeWait.clear();
    if (this.mesh) this.mesh.stop();
    await this.storage.close();
  }
}
