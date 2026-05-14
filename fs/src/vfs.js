import { EventEmitter } from './event_emitter.js';
import { MemoryStorage } from './memory_storage.js';
import { 
  normalizeSelector, 
  Selector,
  isSelector,
  isString,
  getCID, 
  getSelectorKey, 
  encodeSafe, 
  decodeSafe, 
  encodeJCB, 
  decodeJCB,
  decodeInfo
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
    if (options.schema?.arguments && !Array.isArray(options.schema.arguments)) {
      throw new Error(`VFS Error: Arguments for provider '${pattern}' must be an array to preserve positional order.`);
    }
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

  getCatalog() {
    const catalog = {};
    for (const [path, schema] of this.schemas.entries()) {
        catalog[path] = schema;
    }
    return { catalog, provider: this.id };
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

      // 1. Support new array-based arguments format
      if (Array.isArray(schema.arguments)) {
          for (const argDef of schema.arguments) {
              const val = params[argDef.name];
              const isMissing = val === undefined || val === null;
              
              if (isMissing && argDef.default === undefined) {
                  // If it's an affiliate, it might be a hole, but for VFS-level 
                  // fail-fast, we usually expect them to be bound or have a default.
                  // However, jot/eval might pass holes.
                  // For now, let's only throw if it's NOT an affiliate and NOT optional.
                  const isAffiliate = argDef.affiliate === '$in' || argDef.affiliate === '$out';
                  if (!isAffiliate) {
                      throw new Error(`Missing required parameter '${argDef.name}'`);
                  }
              }

              if (!isMissing) {
                  if (argDef.type === 'jot:number' && typeof val !== 'number') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected number, got ${typeof val}`);
                  }
                  if (argDef.type === 'jot:string' && typeof val !== 'string') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected string, got ${typeof val}`);
                  }
                  if (argDef.type === 'jot:boolean' && typeof val !== 'boolean') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected boolean, got ${typeof val}`);
                  }
              }
          }
      }

      // 2. Support legacy JSON-schema style (used in some tests)
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
        expiresAt: context.expiresAt || (Date.now() + 10000) 
    };
    if (Date.now() > packetContext.expiresAt) return null;

    const result = await this._readResult(target, packetContext);
    if (!result || !result.success) {
      if (context.followLinks === false) return null;
      throw new Error(`VFS Read Failure: ${result?.error || 'Content not found'}`);
    }
    if (result.cid === null) return null;

    const stream = await this.storage.get(result.cid);
    return { stream, metadata: result.metadata };
  }

  async write(target, streamOrBytes, context = {}) {
    this._checkClosed();
    // Protocol Integrity: Prevent leaking request objects into storage
    if (streamOrBytes && streamOrBytes.constructor && streamOrBytes.constructor.name === 'IncomingMessage') {
        throw new Error(`CRITICAL PROTOCOL VIOLATION: VFS.write received an 'IncomingMessage' request object as data. This indicates an unconsumed request stream.`);
    }

    let s = null;
    let cid = null;

    if (isString(target)) {
        cid = target.toString();
    } else {
        s = normalizeSelector(target);
        cid = await getSelectorKey(s);
    }
    
    let bytes, dataCID;
    let encoding = context.encoding || 'bytes';

    if (streamOrBytes instanceof Uint8Array) {
        bytes = streamOrBytes;
        dataCID = await getCID(bytes);
    } else if (typeof streamOrBytes === 'string') {
        bytes = new TextEncoder().encode(streamOrBytes);
        dataCID = await getCID(streamOrBytes);
        encoding = context.encoding || 'string';
    } else if (streamOrBytes === null) {
        bytes = new Uint8Array();
        dataCID = await getCID(bytes);
        encoding = context.encoding || 'null';
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
    } else if (isSelector(streamOrBytes)) {
        return this.link(target, streamOrBytes);
    } else {
        dataCID = await getCID(streamOrBytes);
        bytes = new TextEncoder().encode(JSON.stringify(streamOrBytes));
        encoding = context.encoding || 'json';
    }

    if (context.size !== undefined && bytes.length !== context.size) {
        throw new Error(`VFS.write: Size mismatch. Expected ${context.size} bytes, got ${bytes.length}`);
    }

    // THE GATEKEEPER: Persistent metadata MUST be a pure manifest
    const info = {
        state: 'AVAILABLE',
        encoding,
        ...(s ? { selector: s.toJSON() } : {})
    };

    await this.storage.set(cid, bytes, info);

    if (s) {
        this.events.emit('state', { selector: s, cid, state: 'AVAILABLE' });
    }

    return { cid };
  }

  async link(src, tgt) {
    this._checkClosed();
    const s_src = normalizeSelector(src);
    const s_tgt = normalizeSelector(tgt);
    const sourceKey = await getSelectorKey(s_src);
    
    const info = { 
        state: 'AVAILABLE', 
        encoding: 'link',
        selector: s_src.toJSON() 
    };
    const targetData = new TextEncoder().encode(JSON.stringify(s_tgt.toJSON()));
    
    await this.storage.set(sourceKey, targetData, info);
    this.events.emit('state', { selector: s_src, cid: sourceKey, state: 'AVAILABLE' });
    return { cid: sourceKey };
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

    if (resolutionStack.includes(targetCID)) {
         throw new Error(`Circular link detected for ${targetCID}`);
    }
    
    if (this.activeWait.has(targetCID)) return await this.activeWait.get(targetCID);

    const workPromise = (async () => {
      try {
        if (await this.storage.has(targetCID)) {
          const info = await this._getStorageInfo(targetCID);
          if (followLinks && info?.encoding === 'link') {
            const stream = await this.storage.get(targetCID);
            if (!stream) throw new Error(`VFS Link Failure: Missing data for link ${targetCID}`);
            
            const reader = stream.getReader();
            const chunks = [];
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
            }
            const len = chunks.reduce((acc, c) => acc + c.length, 0);
            const bytes = new Uint8Array(len);
            let offset = 0;
            for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
            
            const targetObj = JSON.parse(new TextDecoder().decode(bytes));
            const s_target = Selector.fromObject(targetObj);

            return await this._readResult(s_target, { 
                ...context, 
                depth: depth + 1, 
                resolutionStack: [...resolutionStack, targetCID] 
            });
          } else {
            return { success: true, cid: targetCID, metadata: info || {} };
          }
        }

        const isBackflow = stack.includes(this.id);
        const nextStack = [...stack, this.id]; 

        let resultData = null, meshInfo = {};
        if (s) {
            const provider = this.providers.get(s.path);
            if (provider) resultData = await provider(this, s, context);
        }

        if (resultData === null && this.mesh && !isBackflow) {
          const meshResponse = await this.mesh.read(s || targetCID, { 
              ...context, 
              stack: nextStack, 
              resolutionStack 
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
                  meshInfo = decodeInfo(meshResponse.headers.get('x-vfs-info'));
              }
          }
        } else if (isBackflow) {
            return { success: false, error: 'Backflow' };
        }

        if (resultData !== null && resultData !== undefined) {
          if (s && isSelector(resultData)) {
              await this.link(s, resultData);
              return await this._readResult(s, context);
          }

          if (s) {
              const { cid } = await this.write(s, resultData, { ...meshInfo, ...context });
              const meta = await this._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          } else {
              const { cid } = await this.write(targetCID, resultData, { ...meshInfo, ...context, state: 'AVAILABLE' });
              const meta = await this._getStorageInfo(cid);
              return { success: true, cid, metadata: meta || {} };
          }
        }
        return { success: true, cid: null };
      } catch (err) {
        console.error(`[VFS ${this.id}] _readResult Error:`, err);
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
    if (typeof this.storage.getMeta === 'function') {
      info = await this.storage.getMeta(key);
    } else if (typeof this.storage.iterateMeta === 'function') {
      for await (const entry of this.storage.iterateMeta()) {
        if (entry.cid === key || entry.info?.cid === key) {
            info = entry.info;
            break;
        }
      }
    }
    
    if (info) {
        if (info.selector && !(info.selector instanceof Selector)) {
            info.selector = Selector.fromObject(info.selector);
        }
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
