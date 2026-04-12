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
    let expectedSize = info?.size;

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
            finalData = stream;
        }
        
        if (!finalData) {
            const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
            if (expectedSize !== undefined && totalLength !== expectedSize) {
                throw new Error(`Stream aborted or corrupted: Expected ${expectedSize} bytes, got ${totalLength}`);
            }
            finalData = new Uint8Array(totalLength);
            let offset = 0;
            for (const chunk of chunks) {
                finalData.set(chunk, offset);
                offset += chunk.length;
            }
        }
    }

    if (expectedSize !== undefined && finalData.length !== expectedSize) {
        throw new Error(`Data size mismatch: Expected ${expectedSize} bytes, got ${finalData.length}`);
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
  async *iterateMeta() {
      for (const [cid, entry] of this.results.entries()) {
          yield { cid, info: entry.info };
      }
  }
}

export class VFS {
  constructor(options = {}) {
    this.id = options.id || crypto.randomUUID();
    this.storage = options.storage || new MemoryStorage();
    this.getCID = options.getCID || (async (s) => {
      const normalized = normalizeSelector(s);
      return crypto.createHash('sha256').update(JSON.stringify(normalized)).digest('hex');
    });
    this.providers = new Map();
    this.schemas = new Map();
    this.activeWait = new Map(); // SelectorKey -> Promise<{success: boolean, cid: string}>
    this.events = new EventEmitter();
    this.mesh = null;
    this.closed = false;
  }

  registerProvider(path, handler, options = {}) {
    this.providers.set(path, handler);
    if (options.schema) {
      this.schemas.set(path, options.schema);
      this.writeData('sys/schema', { target: path }, options.schema).catch(e => console.warn(`[VFS ${this.id}] Schema write failed:`, e));
    }
  }

  addSchema(path, schema) {
    this.schemas.set(path, schema);
  }

  validateSelector(s) {
    const schema = this.schemas.get(s.path);
    if (!schema) return true;
    const params = s.parameters || {};
    if (schema.required) {
      for (const req of schema.required) {
        if (params[req] === undefined) throw new Error(`Underconstrained selector for ${s.path}: Missing required parameter '${req}'`);
      }
    }
    return true;
  }

  async read(pathOrSelector, parameters, context = {}) {
    const s = normalizeSelector(pathOrSelector, parameters);
    this.validateSelector(s);
    const { stack = [], expiresAt = Date.now() + 30000, followLinks = true } = context;
    if (Date.now() > expiresAt) return null;

    const key = JSON.stringify(s);
    this.events.emit('trace', { type: 'READ_START', selector: s, stack });

    if (this.mesh) {
        const topic = `${s.path}?${JSON.stringify(s.parameters)}`;
        this.mesh.subscribe(topic, expiresAt, stack).catch(() => {});
    }

    if (this.activeWait.has(key)) {
        const result = await this.activeWait.get(key);
        return result.success ? this.storage.get(result.cid) : null;
    }

    const workPromise = (async () => {
        const cid = await this.getCID(s);
        try {
            if (await this.storage.has(cid)) return { success: true, cid };

            let provider = this.providers.get(s.path);
            if (!provider) {
                for (const [pattern, handler] of this.providers.entries()) {
                    if (pattern.startsWith('.') && s.path.endsWith(pattern)) { provider = handler; break; }
                    if (pattern.endsWith('/') && s.path.startsWith(pattern)) { provider = handler; break; }
                }
            }

            let resultData = null;
            let resultInfo = { path: s.path, parameters: s.parameters };

            if (provider) {
                this.events.emit('trace', { type: 'HANDLER_START', selector: s, peer: this.id });
                resultData = await provider(this, s, { ...context, expiresAt });
            } else if (this.mesh) {
                this.events.emit('trace', { type: 'MESH_START', selector: s, peer: this.id });
                const meshResponse = await this.mesh.read(s.path, s.parameters, { stack, expiresAt });
                if (meshResponse) {
                    const headers = meshResponse.headers;
                    let size = null;
                    if (headers) {
                        if (typeof headers.get === 'function') size = headers.get('Content-Length');
                        else if (headers instanceof Map) size = headers.get('Content-Length');
                        else size = headers['Content-Length'] || headers['content-length'];
                    }
                    if (size) resultInfo.size = parseInt(size, 10);
                    resultData = meshResponse.body || meshResponse;
                }
            }

            if (resultData) {
                if (followLinks) {
                    const peekResult = await this._peekLink(resultData);
                    if (peekResult.isLink) {
                        const linkMetadata = await this._extractMetadata(peekResult.text);
                        Object.assign(resultInfo, linkMetadata);
                        const linkedStream = await this.read(peekResult.linkPath, peekResult.linkParams || s.parameters, { 
                            ...context, stack: [...stack, this.id] 
                        });
                        if (linkedStream) resultData = linkedStream;
                    } else {
                        resultData = peekResult.stream;
                    }
                }

                try {
                    await this.write(s.path, s.parameters, resultData, resultInfo);
                    return { success: true, cid };
                } catch (err) {
                    return { success: false, cid };
                }
            }
            return { success: false, cid };
        } catch (err) {
            this.events.emit('trace', { type: 'ERROR', selector: s, message: err.message, peer: this.id });
            return { success: false, cid: '' };
        } finally {
            this.activeWait.delete(key);
            this.events.emit('trace', { type: 'READ_END', selector: s });
        }
    })();

    this.activeWait.set(key, workPromise);
    const result = await workPromise;
    return result.success ? this.storage.get(result.cid) : null;
  }

  async _extractMetadata(jsonOrText) {
      try {
          const j = typeof jsonOrText === 'string' ? JSON.parse(jsonOrText) : jsonOrText;
          const metadata = {};
          if (j.tags) metadata.tags = j.tags;
          return metadata;
      } catch (e) { return {}; }
  }

  async _peekLink(stream) {
      if (stream instanceof Uint8Array || typeof stream === 'string') {
          const text = (typeof stream === 'string') ? stream : new TextDecoder().decode(stream);
          try {
              const j = JSON.parse(text);
              if (j.geometry && typeof j.geometry === 'string' && j.geometry.startsWith('vfs:/')) {
                  return { isLink: true, linkPath: j.geometry.slice(5), linkParams: j.parameters, stream, text };
              }
          } catch(e) {}
          return { isLink: false, stream, text: null };
      }

      const reader = stream.getReader();
      const { done, value } = await reader.read();
      if (done || !value) { reader.releaseLock(); return { isLink: false, stream, text: null }; }

      let text = null;
      try {
          text = new TextDecoder().decode(value);
          const j = JSON.parse(text);
          if (j.geometry && typeof j.geometry === 'string' && j.geometry.startsWith('vfs:/')) {
              reader.releaseLock();
              return { isLink: true, linkPath: j.geometry.slice(5), linkParams: j.parameters, stream: null, text };
          }
      } catch(e) {}

      return {
          isLink: false, text,
          stream: new ReadableStream({
              async start(controller) {
                  controller.enqueue(value);
                  try {
                      while (true) {
                          const { done, value: nextValue } = await reader.read();
                          if (done) break;
                          controller.enqueue(nextValue);
                      }
                  } finally { reader.releaseLock(); controller.close(); }
              }
          })
      };
  }

  async write(pathOrSelector, parameters, stream, info = {}) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    try {
        await this.storage.set(cid, stream, { ...info, path: s.path, parameters: s.parameters });
        this.events.emit('state', { path: s.path, parameters: s.parameters, cid, state: 'AVAILABLE' });
    } catch (err) {
        await this.storage.delete(cid);
        throw err;
    }
  }

  async writeData(pathOrSelector, parameters, data) {
    let bytes;
    if (data instanceof Uint8Array) bytes = data;
    else if (typeof data === 'string') bytes = new TextEncoder().encode(data);
    else bytes = new TextEncoder().encode(JSON.stringify(data, null, 2));
    const stream = new WebReadableStream({
      start(controller) { controller.enqueue(bytes); controller.close(); },
    });
    await this.write(pathOrSelector, parameters, stream);
  }

  async readData(pathOrSelector, parameters, context = {}) {
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
    } finally { reader.releaseLock(); }
    let len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    const text = new TextDecoder().decode(bytes);
    try {
        const trimmed = text.trim();
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) return JSON.parse(trimmed);
    } catch(e) {}
    return bytes;
  }

  async close() {
    this.closed = true;
    this.activeWait.clear();
    if (this.mesh) this.mesh.stop();
    await this.storage.close();
  }

  _checkClosed() { if (this.closed) throw new VFSClosedError(); }

  static formatVFSChunk(selector, dataBytes = new Uint8Array(0)) {
      const name = JSON.stringify(normalizeSelector(selector));
      const headerStr = `\n=${dataBytes.length} ${name}\n`;
      const headerBytes = new TextEncoder().encode(headerStr);
      const combined = new Uint8Array(headerBytes.length + dataBytes.length);
      combined.set(headerBytes);
      combined.set(dataBytes, headerBytes.length);
      return combined;
  }

  static async *parseVFSBundle(stream) {
      if (!stream) return;
      const reader = typeof stream.getReader === 'function' ? stream.getReader() : null;
      let buffer = new Uint8Array(0);
      const append = (chunk) => {
          const newBuffer = new Uint8Array(buffer.length + chunk.length);
          newBuffer.set(buffer); newBuffer.set(chunk, buffer.length);
          buffer = newBuffer;
      };
      const decoder = new TextDecoder();
      try {
          while (true) {
              if (reader) {
                  const { done, value } = await reader.read();
                  if (value) append(value);
                  if (done && buffer.length === 0) break;
              } else break;

              let offset = 0;
              while (offset < buffer.length) {
                  if (buffer[offset] === 10 && buffer[offset + 1] === 61) {
                      let newlineIdx = -1;
                      for (let i = offset + 2; i < buffer.length; i++) {
                          if (buffer[i] === 10) { newlineIdx = i; break; }
                      }
                      if (newlineIdx !== -1) {
                          const headerStr = decoder.decode(buffer.subarray(offset + 2, newlineIdx));
                          const spaceIdx = headerStr.indexOf(' ');
                          if (spaceIdx !== -1) {
                              const len = parseInt(headerStr.slice(0, spaceIdx), 10);
                              const nameStr = headerStr.slice(spaceIdx + 1);
                              let selector; try { selector = JSON.parse(nameStr); } catch (e) { selector = { path: nameStr }; }
                              const payloadStart = newlineIdx + 1;
                              if (buffer.length >= payloadStart + len) {
                                  const payload = buffer.subarray(payloadStart, payloadStart + len);
                                  yield { selector, data: new Uint8Array(payload) };
                                  offset = payloadStart + len;
                                  continue;
                              }
                          }
                      }
                  } else if (buffer[offset] !== 10) { offset++; continue; }
                  break;
              }
              if (offset > 0) buffer = buffer.slice(offset);
              if (reader && buffer.length === 0) break;
          }
      } finally { if (reader) reader.releaseLock(); }
  }

  async spy(pathOrSelector, parameters, context = {}) {
      const s = normalizeSelector(pathOrSelector, parameters);
      const { stack = [], expiresAt = Date.now() + 30000 } = context;
      const chunks = [];
      const seenCIDs = new Set();
      if (typeof this.storage.iterateMeta === "function") {
          for await (const { cid, info } of this.storage.iterateMeta()) {
              if (info && this._matchesSpy(info, s)) {
                  if (!seenCIDs.has(cid)) {
                      seenCIDs.add(cid);
                      const stream = await this.storage.get(cid);
                      if (stream) {
                          const bytes = await this._streamToBytes(stream);
                          chunks.push(VFS.formatVFSChunk(info, bytes));
                      }
                  }
              }
          }
      }
      if (this.mesh) {
          const meshStream = await this.mesh.spy(s.path, s.parameters, { stack, expiresAt });
          if (meshStream) {
              const reader = meshStream.getReader();
              try {
                  while (true) {
                      const { done, value } = await reader.read();
                      if (done) break;
                      chunks.push(value);
                  }
              } finally { reader.releaseLock(); }
          }
      }
      if (chunks.length === 0) return null;
      return new WebReadableStream({
          start(controller) { for (const chunk of chunks) controller.enqueue(chunk); controller.close(); }
      });
  }

  async _streamToBytes(stream) {
      if (stream instanceof Uint8Array) return stream;
      const chunks = [];
      const reader = stream.getReader();
      try {
          while (true) {
              const { done, value } = await reader.read();
              if (done) break;
              chunks.push(value);
          }
      } finally { reader.releaseLock(); }
      let len = chunks.reduce((acc, c) => acc + c.length, 0);
      const bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
      return bytes;
  }
}
