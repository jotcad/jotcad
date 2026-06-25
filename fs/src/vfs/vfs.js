import { EventEmitter } from '../event_emitter.js';
import { MemoryStorage } from '../memory_storage.js';
import { log } from '../log.js';
import { 
  normalizeSelector, 
  Selector,
  isSelector,
  isString,
  getCID, 
  getSelectorKey
} from '../cid.js';
import { validateSelector } from './schema.js';
import { _readResult } from './resolver.js';

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
    log(`[VFS ${this.id}] Registering provider for: ${pattern}`);
    if (options.schema?.arguments && !Array.isArray(options.schema.arguments)) {
      throw new Error(`VFS Error: Arguments for provider '${pattern}' must be an array to preserve positional order.`);
    }
    this.providers.set(pattern, handler);
    if (options.schema) this.schemas.set(pattern, options.schema);
    if (this.mesh && typeof this.mesh.registerOp === 'function') {
      this.mesh.registerOp(pattern);
    }
  }

  addSchema(path, schema) { this.schemas.set(path, schema); }

  _findProvider(path) {
    if (this.providers.has(path)) return this.providers.get(path);
    for (const [pattern, handler] of this.providers.entries()) {
        if (pattern.endsWith('*') && path.startsWith(pattern.slice(0, -1))) return handler;
    }
    return null;
  }

  getCatalog() {
    const catalog = {};
    for (const [path, schema] of this.schemas.entries()) catalog[path] = schema;
    return { catalog, provider: this.id };
  }

  validateSelector(selector) { validateSelector(this.schemas, selector); }

  async _getStorageInfo(cid) {
    const info = await this.storage.getMeta(cid);
    return info || null;
  }

  async unnestSelector(selector, tasks = []) {
    if (!isSelector(selector)) return { selector, tasks };

    const newParams = {};
    for (const [key, val] of Object.entries(selector.parameters)) {
      if (isSelector(val)) {
        const { selector: childSel } = await this.unnestSelector(val, tasks);
        const childCID = await getSelectorKey(childSel);
        newParams[key] = childCID;
        tasks.push({
          cid: childCID,
          selector: childSel,
          fn: () => this.readSelector(childSel, { unnested: true })
        });
      } else if (Array.isArray(val)) {
        const newVal = [];
        for (const item of val) {
          if (isSelector(item)) {
            const { selector: childSel } = await this.unnestSelector(item, tasks);
            const childCID = await getSelectorKey(childSel);
            newVal.push(childCID);
            tasks.push({
              cid: childCID,
              selector: childSel,
              fn: () => this.readSelector(childSel, { unnested: true })
            });
          } else {
            newVal.push(item);
          }
        }
        newParams[key] = newVal;
      } else if (val && typeof val === 'object' && val.constructor === Object) {
        const newObj = {};
        for (const [k, v] of Object.entries(val)) {
          if (isSelector(v)) {
            const { selector: childSel } = await this.unnestSelector(v, tasks);
            const childCID = await getSelectorKey(childSel);
            newObj[k] = childCID;
            tasks.push({
              cid: childCID,
              selector: childSel,
              fn: () => this.readSelector(childSel, { unnested: true })
            });
          } else {
            newObj[k] = v;
          }
        }
        newParams[key] = newObj;
      } else {
        newParams[key] = val;
      }
    }

    const resultSelector = new Selector(selector.path, newParams);
    if (selector.output) resultSelector.output = selector.output;
    return { selector: resultSelector, tasks };
  }

  async readSelector(selector, context = {}) {
    this._checkClosed();
    let s = normalizeSelector(selector);
    if (!context.unnested && (!context.stack || context.stack.length === 0)) {
      const { selector: unnestedSelector, tasks } = await this.unnestSelector(s);
      s = unnestedSelector;
      for (const t of tasks) {
        await t.fn();
      }
    }
    const packetContext = { 
        ...context, 
        stack: context.stack || [], 
        resolutionStack: context.resolutionStack || [],
        // TODO: Investigate why multi-hop recursive mesh reads take > 10s during initialization.
        expiresAt: context.expiresAt || (Date.now() + 30000) 
    };
    if (Date.now() > packetContext.expiresAt) return null;

    const result = await _readResult(this, s, packetContext);
    if (!result || !result.success) {
      const errMsg = result?.error || 'Content not found';
      log(`[VFS ${this.id}] Read failed: ${errMsg}`);
      throw new Error(`VFS Read Failed: ${errMsg}`);
    }
    if (result.cid === null) return null;

    const stream = await this.storage.get(result.cid);
    return { stream, metadata: result.metadata };
  }

  async readCID(cid, context = {}) {
    this._checkClosed();
    if (!isString(cid)) throw new Error('VFS.readCID requires a string CID');
    
    const packetContext = { 
        ...context, 
        stack: context.stack || [], 
        resolutionStack: context.resolutionStack || [],
        // TODO: Investigate why multi-hop recursive mesh reads take > 10s during initialization.
        expiresAt: context.expiresAt || (Date.now() + 30000) 
    };
    if (Date.now() > packetContext.expiresAt) return null;

    const result = await _readResult(this, cid, packetContext);
    if (!result || !result.success) {
      const errMsg = result?.error || 'Content not found';
      log(`[VFS ${this.id}] Read failed: ${errMsg}`);
      throw new Error(`VFS Read Failed: ${errMsg}`);
    }
    if (result.cid === null) return null;

    const stream = await this.storage.get(result.cid);
    return { stream, metadata: result.metadata };
  }

  /**
   * Legacy method for backward compatibility. 
   * @deprecated Use readSelector() or readCID() instead.
   */
  async read(target, context = {}) {
    if (isString(target)) return this.readCID(target, context);
    return this.readSelector(target, context);
  }

  async write(target, streamOrBytes, context = {}) {
    this._checkClosed();
    log(`[VFS ${this.id}] write(${target.path || target})`);
    
    let s = null, cid = null;
    if (isString(target)) { cid = target.toString(); } 
    else { s = normalizeSelector(target); cid = await getSelectorKey(s); }
    
    let bytes, dataCID, encoding = context.encoding || 'bytes';

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
        log(`[VFS ${this.id}] write: Input is ReadableStream, draining...`);
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

    const info = {
      state: 'AVAILABLE',
      encoding,
      ...(s ? { selector: s.toJSON() } : {}),
      ...(context.filename ? { filename: context.filename } : {})
    };
    await this.storage.set(cid, bytes, info);
    if (s) this.events.emit('state', { selector: s, cid, state: 'AVAILABLE' });
    return { cid };
  }

  async link(src, tgt) {
    this._checkClosed();
    const s_src = normalizeSelector(src), s_tgt = normalizeSelector(tgt);
    const sourceKey = await getSelectorKey(s_src);
    const info = { state: 'AVAILABLE', encoding: 'link', selector: s_src.toJSON() };
    const targetData = new TextEncoder().encode(JSON.stringify(s_tgt.toJSON()));
    await this.storage.set(sourceKey, targetData, info);
    this.events.emit('state', { selector: s_src, cid: sourceKey, state: 'AVAILABLE' });
    return { cid: sourceKey };
  }

  async spy(selector, context = {}) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    const { stack = [], expiresAt = Date.now() + 10000 } = context;

    const results = [];
    // 1. Scan Local Storage
    for await (const entry of this.storage.iterateMeta()) {
        const { cid, info } = entry;
        if (info.selector && info.selector.path) {
            const path = info.selector.path;
            const pattern = s.path;
            const match = pattern === '*' || 
                        (pattern.endsWith('*') && path.startsWith(pattern.slice(0, -1))) || 
                        pattern === path;
            if (match) results.push({ cid, info });
        }
    }

    const localStream = results.length > 0 ? new ReadableStream({
        start(controller) {
            const encoder = new TextEncoder();
            for (const r of results) {
                const metadata = JSON.stringify(r.info);
                const header = `\n=${metadata.length} ${r.cid}\n`;
                controller.enqueue(encoder.encode(header + metadata));
            }
            controller.close();
        }
    }) : null;

    // 2. Scan Mesh
    let meshStream = null;
    if (this.mesh && !stack.includes(this.id)) {
        meshStream = await this.mesh.spy(s, { ...context, stack: [...stack, this.id] });
    }

    if (!localStream && !meshStream) return null;
    if (localStream && !meshStream) return localStream;
    if (!localStream && meshStream) return meshStream;

    // Multiplex both
    return new ReadableStream({
        async start(controller) {
            const readers = [localStream.getReader(), meshStream.getReader()];
            for (const reader of readers) {
                try {
                    while (true) {
                        const { done, value } = await reader.read();
                        if (done) break;
                        controller.enqueue(value);
                    }
                } finally { reader.releaseLock(); }
            }
            controller.close();
        }
    });
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = [], callback = null) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    
    let unsubscribe = () => {};
    if (callback) {
      const listener = (notifiedSelector, payload) => {
        if (notifiedSelector.equals(s)) {
          callback(notifiedSelector, payload);
        }
      };
      this.events.on('notify', listener);
      unsubscribe = () => this.events.off('notify', listener);
    }

    if (this.mesh) this.mesh.subscribe(s, expiresAt, stack);
    return unsubscribe;
  }

  async notify(selector, payload, stack = []) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    
    // 1. Local Delivery
    this.events.emit('notify', s, payload);

    // 2. Mesh Propagation
    if (this.mesh) this.mesh.notify(s, payload, stack);
  }

  async close() {
    this.closed = true;
    if (this.mesh && typeof this.mesh.stop === 'function') {
      await this.mesh.stop();
    }
    await this.storage.close();
  }
}
