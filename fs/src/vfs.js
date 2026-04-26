import { EventEmitter } from './event_emitter.js';
import { MemoryStorage } from './memory_storage.js';
import { 
  normalizeSelector, 
  Selector,
  isString,
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
    console.log(`[VFS ${this.id}] Registering provider for: ${pattern}`);
    this.providers.set(pattern, handler);
    if (options.schema) {
        this.schemas.set(pattern, options.schema);
    }
  }

  _hasProvider(path) {
    if (this.providers.has(path)) return true;
    for (const pattern of this.providers.keys()) {
        if (pattern.endsWith('*') && path.startsWith(pattern.slice(0, -1))) return true;
    }
    return false;
  }

  _matches(s, t) {
    if (!s || !t || !t.selector) return false;
    const ts = t.selector;
    if (s.path.endsWith('*')) {
      if (!ts.path.startsWith(s.path.slice(0, -1))) return false;
    } else if (s.path !== ts.path) return false;
    for (const [k, v] of Object.entries(s.parameters)) {
      if (JSON.stringify(v) !== JSON.stringify(ts.parameters[k])) return false;
    }
    return true;
  }

  addSchema(path, schema) {
    this.schemas.set(path, schema);
  }

  validateSelector(selector) {
    if (typeof selector === 'string' && /^[0-9a-f]{64}$/i.test(selector)) return;
    if (!selector || typeof selector !== 'object') throw new Error('Invalid selector object');
    
    // Parity with C++: path is required and must not be empty
    if (!selector.path || typeof selector.path !== 'string') {
        throw new Error('Schema Violation: Selector path cannot be empty.');
    }

    // Parity with C++: parameters must be an object and cannot be null
    if (selector.parameters === null || typeof selector.parameters !== 'object') {
        throw new Error('Schema Violation: Selector parameters must be an object.');
    }
    
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

  async read(target, context = {}) {
    this._checkClosed();
    const packetContext = { 
        ...context, 
        stack: context.stack || [], 
        resolutionStack: context.resolutionStack || [],
        expiresAt: context.expiresAt || Date.now() + 10000 
    };
    if (Date.now() > packetContext.expiresAt) return null;

    const result = await this._readResult(target, packetContext);
    if (!result || !result.success) {
      if (context.followLinks === false) return null;
      throw new Error(`VFS Read Failure: ${result?.error || 'Content not found'}`);
    }
    if (result.cid === null) return null;

    return this.storage.get(result.cid);
  }

  async write(target, streamOrBytes, context = {}) {
    this._checkClosed();
    let s = null;
    let cid = null;

    if (isString(target)) {
        cid = target.toString();
    } else {
        s = normalizeSelector(target);
        cid = await getSelectorKey(s);
    }
    
    let bytes, dataCID, type = 'bytes';

    if (streamOrBytes instanceof Uint8Array) {
        bytes = streamOrBytes;
        dataCID = await getCID(bytes);
    } else if (typeof streamOrBytes === 'string') {
        bytes = new TextEncoder().encode(streamOrBytes);
        dataCID = await getCID(streamOrBytes);
        type = 'string';
    } else if (streamOrBytes === null) {
        bytes = new Uint8Array();
        dataCID = await getCID(bytes);
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
        dataCID = await getCID(bytes);
    } else {
        dataCID = await getCID(streamOrBytes);
        bytes = new TextEncoder().encode(JSON.stringify(streamOrBytes));
        type = 'json';
    }

    const info = {
        type,
        ...context,
        state: 'AVAILABLE',
        cid
    };
    if (s) info.selector = s;
    if (type === 'json' && info.type !== 'bytes') info.type = 'json';
    delete info.expiresAt;

    // Proper Linkage: Only store literal data in metadata if it's not a stream
    if ((type === 'json' || type === 'string') && !(streamOrBytes && typeof streamOrBytes.getReader === 'function')) {
        info.data = streamOrBytes;
    }
    
    // Store by the CID derived from the Selector
    await this.storage.set(cid, bytes, info);
    
    // If it's heavy data (Geometry), also store it by its data hash for deduplication
    if (type === 'bytes' || type === 'string') {
        await this.storage.set(dataCID, bytes, { state: 'AVAILABLE', type });
    }

    if (s) {
        this.events.emit('state', { selector: s, cid, state: 'AVAILABLE' });
    }

    return { cid };
  }

  async writeData(selector, data, context = {}) { return await this.write(selector, data, context); }

  async readData(target, context = {}) {
    if (!target) throw new Error('VFS.readData: Missing required target (CID or Selector)');
    const result = await this._readResult(target, { ...context, stack: [], resolutionStack: context.resolutionStack || [] });
    if (!result || !result.success) {
        throw new Error(`VFS ReadData Failure: ${result?.error || 'Content not found'}`);
    }
    if (result.cid === null) return null;
    
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
    const meta = result.metadata || {};

    // Protocol Invariant Check: 0-byte artifacts marked AVAILABLE are a terminal error
    if (len === 0 && meta.state === 'AVAILABLE' && meta.type !== 'null') {
        throw new Error(`CRITICAL PROTOCOL ERROR: Stream Ownership Bug. CID ${result.cid.slice(0, 8)} returned 0 bytes but is marked AVAILABLE. This usually means a stream was double-consumed.`);
    }

    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    
    // console.log(`[VFS] readData(${result.cid.slice(0,8)}) meta.type:`, meta.type);
    if (meta.type === 'null') return null;
    
    // Check for raw JCB binary first
    if (bytes[0] === 6 || bytes[0] === 5) {
        try {
            return decodeJCB(bytes);
        } catch (e) {
            console.error('[VFS] decodeJCB failed:', e.message, 'First 10 bytes:', bytes.slice(0, 10));
        }
    }

    if (meta.type === 'bytes') {
        return bytes;
    } else if (meta.type === 'json') {
        if (meta.data && typeof meta.data === 'object' && !(meta.data.getReader)) {
            return meta.data;
        }
        try {
            return JSON.parse(new TextDecoder().decode(bytes));
        } catch (e) {
            return bytes;
        }
    } else if (meta.type === 'string') {
        if (typeof meta.data === 'string') return meta.data;
        return new TextDecoder().decode(bytes);
    }
    
    return bytes;
  }

  async readText(target, context = {}) {
    const data = await this.readData(target, context);
    if (!data) return null;
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
    const targetKey = await getSelectorKey(s_src);
    const info = { selector: s_src, target: s_tgt, state: 'LINKED' };
    const linkText = `vfs:/${s_tgt.path}${Object.keys(s_tgt.parameters).length ? '?' + JSON.stringify(s_tgt.parameters) : ''}`;
    await this.storage.set(targetKey, new TextEncoder().encode(linkText), info);
    this.events.emit('state', { selector: s_src, cid: targetKey, state: 'LINKED', target: s_tgt });
  }

  async _readResult(target, context = {}) {
    const { 
        stack = [], 
        resolutionStack = [],
        expiresAt = Date.now() + 10000, 
        followLinks = true, 
        depth = 0 
    } = context;
    
    let s = null;
    let targetCID = null;

    if (isString(target)) {
        targetCID = target.toString();
    } else {
        s = normalizeSelector(target);
        this.validateSelector(s);
        targetCID = await getSelectorKey(s);
    }

    if (depth > 20) throw new Error(`Maximum recursion depth exceeded for ${targetCID}`);
    if (Date.now() > expiresAt) return { success: false, error: 'Expired' };

    // Protocol Cycle Protection: resolutionStack contains target CIDs to detect circular links.
    if (resolutionStack.includes(targetCID)) {
         throw new Error(`Circular link detected for ${targetCID}`);
    }
    
    if (this.activeWait.has(targetCID)) return await this.activeWait.get(targetCID);

    const workPromise = (async () => {
      try {
        if (await this.storage.has(targetCID)) {
          const info = await this._getStorageInfo(targetCID);
          if (followLinks && info?.state === 'LINKED' && info?.target) {
            // Resolve Link: Pass updated resolutionStack to the recursion
            return await this._readResult(info.target, { 
                ...context, 
                depth: depth + 1, 
                resolutionStack: [...resolutionStack, targetCID] 
            });
          } else {
            return { success: true, cid: targetCID, metadata: info || {} };
          }
        }

        // Remote packet logic
        const isBackflow = stack.includes(this.id);
        const nextStack = [...stack, this.id]; 

        let resultData = null, meshInfo = {};
        if (s) {
            // Provider Lookup
            const provider = this.providers.get(s.path);
            if (provider) {
                resultData = await provider(this, s, context);
            }
        }

        if (!resultData && this.mesh && (!isBackflow || s)) {
          const meshResponse = await this.mesh.read(s || targetCID, { 
              ...context, 
              stack: nextStack, 
              resolutionStack // Pass current stack as-is (no new addition here)
          });
          if (meshResponse) {
              const rawBody = meshResponse.body || meshResponse;
              if (rawBody instanceof ReadableStream) {
                  const chunks = [];
                  const reader = rawBody.getReader();
                  while (true) {
                      const { done, value } = await reader.read();
                      if (done) break;
                      chunks.push(value);
                  }
                  const len = chunks.reduce((acc, c) => acc + c.length, 0);
                  resultData = new Uint8Array(len);
                  let offset = 0;
                  for (const chunk of chunks) { resultData.set(chunk, offset); offset += chunk.length; }
              } else {
                  resultData = rawBody;
              }

              if (meshResponse.headers) {
                  const infoHeader = meshResponse.headers.get('x-vfs-info');
                  if (infoHeader) { try { meshInfo = decodeSafe(infoHeader); } catch (e) {} }
              }
          }
        } else if (isBackflow) {
            return { success: false, error: 'Backflow' };
        }

        if (resultData) {
          // If we have a selector, write it properly to its computational CID
          if (s) {
              const { cid } = await this.write(s, resultData, { ...meshInfo, ...context });
              const meta = await this._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          } else {
              // Direct CID write (anonymous)
              // Ensure we use the specialized writeData which handles streams and metadata
              const { cid } = await this.writeData(targetCID, resultData, { ...meshInfo, ...context, state: 'AVAILABLE' });
              const meta = await this._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          }
        }
        return { success: true, cid: null };
      } catch (err) {
        return { success: false, error: err.message };
      } finally { this.activeWait.delete(targetCID); }
    })();

    this.activeWait.set(targetCID, workPromise);
    return await workPromise;
  }

  async _readResultDirect(selector, context = {}) {
      return await this._readResult(selector, { ...context, followLinks: false });
  }

  async _getStorageInfo(key) {
    let info = null;
    if (typeof this.storage.iterateMeta === 'function') {
      for await (const entry of this.storage.iterateMeta()) {
        if (entry.cid === key || entry.info?.cid === key) {
            info = entry.info;
            break;
        }
      }
    }
    if (info && info.target && !(info.target instanceof Selector)) {
        info.target = new Selector(info.target.path, info.target.parameters, info.target.output);
    }
    return info;
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
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
        const meshStream = await this.mesh.spy(s, { ...context, stack: nextStack, expiresAt });
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
