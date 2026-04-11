/**
 * Simple EventEmitter shim for both Node and Browser.
 */
class EventEmitter {
  constructor() {
    this.listeners = {};
    this.maxListeners = 10;
  }
  on(event, listener) {
    if (!this.listeners[event]) this.listeners[event] = [];
    this.listeners[event].push(listener);
  }
  off(event, listener) {
    if (!this.listeners[event]) return;
    this.listeners[event] = this.listeners[event].filter((l) => l !== listener);
  }
  emit(event, ...args) {
    if (!this.listeners[event]) return;
    this.listeners[event].forEach((l) => l(...args));
  }
  removeAllListeners() {
    this.listeners = {};
  }
  setMaxListeners(n) {
    this.maxListeners = n;
  }
}

/**
 * Normalizes arguments into a standard selector object.
 */
export const normalizeSelector = (pathOrSelector, parameters = {}) => {
  let path = '';
  let params = parameters || {};

  if (typeof pathOrSelector === 'string') {
    path = pathOrSelector;
    if (path.includes('?')) {
        const [p, query] = path.split('?');
        path = p;
        try { params = { ...JSON.parse(decodeURIComponent(query)), ...params }; } catch(e) {}
    }
  } else if (pathOrSelector && typeof pathOrSelector === 'object') {
    path = pathOrSelector.path;
    params = pathOrSelector.parameters || parameters || {};
  } else {
    throw new Error(`Invalid selector: ${pathOrSelector}`);
  }

  const clean = (obj) => {
    if (!obj || typeof obj !== 'object') return obj;
    if (Array.isArray(obj)) return obj.map(clean);
    const result = {};
    const keys = Object.keys(obj).sort();
    for (const k of keys) result[k] = clean(obj[k]);
    return result;
  };

  return { path, parameters: clean(params) };
};

export const isMatch = (s1, s2) => {
  if (!s1 || !s2) return s1 === s2;
  const a = normalizeSelector(s1);
  const b = normalizeSelector(s2);
  if (a.path !== b.path) return false;
  const deepEqual = (p1, p2) => {
    if (p1 === p2) return true;
    if (typeof p1 !== typeof p2) return false;
    if (!p1 || !p2 || typeof p1 !== 'object') return p1 === p2;
    const keysA = Object.keys(p1);
    const keysB = Object.keys(p2);
    if (keysA.length !== keysB.length) return false;
    for (const key of keysA) {
        if (!deepEqual(p1[key], p2[key])) return false;
    }
    return true;
  };
  return deepEqual(a.parameters, b.parameters);
};

export class VFSClosedError extends Error {
  constructor() {
    super('VFS instance has been closed.');
    this.name = 'VFSClosedError';
  }
}

export class MemoryStorage {
  constructor() {
    this.results = new Map();
  }
  async get(cid) {
    const entry = this.results.get(cid);
    if (!entry) return null;
    return new ReadableStream({
      start(c) {
        c.enqueue(entry.data);
        c.close();
      },
    });
  }
  async set(cid, stream, info) {
    const chunks = [];
    if (typeof stream.getReader === 'function') {
        const reader = stream.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
        }
    } else {
        for await (const chunk of stream) chunks.push(chunk);
    }
    const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
    const data = new Uint8Array(totalLength);
    let offset = 0;
    for (const chunk of chunks) {
      data.set(chunk, offset);
      offset += chunk.length;
    }
    this.results.set(cid, { info, data });
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

/**
 * Core VFS logic (Mesh-VFS Edition).
 */
export class VFS {
  constructor(options = {}) {
    this.id = options.id || Math.random().toString(36).slice(2);
    this.getCID = options.getCID;
    this.storage = options.storage || new MemoryStorage();
    this.events = new EventEmitter();
    this.events.setMaxListeners(100);
    this.activeWait = new Map(); // CID -> Promise
    this.providers = new Map(); // Path -> Handler(vfs, selector)
    this.links = new Map(); // CID -> targetSelector
    this.mesh = null; // MeshLink instance
    this.closed = false;
  }

  async close() {
    if (this.closed) return;
    this.closed = true;
    this.events.emit('state', { state: 'CLOSED' });
    this.events.removeAllListeners();
    await this.storage.close();
  }

  _checkClosed() {
    if (this.closed) throw new VFSClosedError();
  }

  registerProvider(path, handler) {
    this.providers.set(path, handler);
  }

  /**
   * Performs a strictly LOCAL read (Disk + Local Providers).
   */
  async localRead(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    
    if (this.links.has(cid)) return this.localRead(this.links.get(cid));
    if (this.activeWait.has(cid)) {
        await this.activeWait.get(cid);
        return this.storage.get(cid);
    }
    
    // Check Disk
    if (await this.storage.has(cid)) {
        return this.storage.get(cid);
    }

    const provider = this.providers.get(s.path);
    if (provider) {
        const workPromise = (async () => {
            try {
                const stream = await provider(this, s);
                if (stream) await this.write(s, stream);
            } catch (err) {
                console.error(`[VFS ${this.id}] Provider error for ${s.path}:`, err);
                throw err;
            }
        })();
        this.activeWait.set(cid, workPromise);
        try {
            await workPromise;
            return this.storage.get(cid);
        } finally {
            this.activeWait.delete(cid);
        }
    }
    return null;
  }

  /**
   * Performs a full READ (Local -> Mesh).
   */
  async read(pathOrSelector, parameters) {
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);

    if (this.activeWait.has(cid)) {
        await this.activeWait.get(cid);
        return this.storage.get(cid);
    }

    const localStream = await this.localRead(s);
    if (localStream) return localStream;

    if (this.mesh) {
        console.log(`[VFS ${this.id}] MISS for ${s.path}. Seeking in mesh...`);
        const meshPromise = this.mesh.read(s.path, s.parameters);
        this.activeWait.set(cid, meshPromise);
        try {
            const meshStream = await meshPromise;
            // The meshStream returned here IS the fresh stream from local storage
            // created by mesh.read after it verified and committed the data.
            return meshStream;
        } finally {
            this.activeWait.delete(cid);
        }
    }

    return null;
  }

  async write(pathOrSelector, parameters, stream) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    await this.storage.set(cid, stream, { path: s.path, parameters: s.parameters });
    this.events.emit('state', { path: s.path, parameters: s.parameters, cid, state: 'AVAILABLE' });
  }

  async link(sourceSelector, targetSelector) {
    const src = normalizeSelector(sourceSelector);
    const tgt = normalizeSelector(targetSelector);
    const cid = await this.getCID(src);
    this.links.set(cid, tgt);
  }

  async readData(pathOrSelector, parameters) {
    const stream = await this.read(pathOrSelector, parameters);
    if (!stream) return null;
    const chunks = [];
    if (typeof stream.getReader === 'function') {
        const reader = stream.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
        }
    } else {
        for await (const chunk of stream) chunks.push(chunk);
    }
    let bytes;
    if (typeof Buffer !== 'undefined') {
      bytes = Buffer.concat(chunks.map((c) => Buffer.from(c)));
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
        if (trimmed.startsWith('{') || trimmed.startsWith('[')) return JSON.parse(trimmed);
    } catch(e) {}
    return text;
  }

  async writeData(pathOrSelector, parameters, data) {
    let bytes;
    if (data instanceof Uint8Array) bytes = data;
    else if (typeof data === 'string') bytes = new TextEncoder().encode(data);
    else bytes = new TextEncoder().encode(JSON.stringify(data, null, 2));

    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      },
    });
    return this.write(pathOrSelector, parameters, stream);
  }

  async declare(path, schema) {
    await this.writeData(`${path}@schema`, {}, schema);
  }

  async *watch(selector, options = {}) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    const queue = [];
    let resolveQueue;
    const onState = (event) => {
      if (event.state === 'CLOSED') {
        queue.push(event);
        if (resolveQueue) { resolveQueue(); resolveQueue = null; }
        return;
      }
      if (isMatch(event, s)) {
          queue.push(event);
          if (resolveQueue) { resolveQueue(); resolveQueue = null; }
      }
    };
    this.events.on('state', onState);
    try {
      while (!this.closed) {
        if (queue.length === 0) await new Promise((resolve) => (resolveQueue = resolve));
        if (this.closed) break;
        while (queue.length > 0) {
          const ev = queue.shift();
          if (ev.state === 'CLOSED') return;
          yield ev;
        }
      }
    } finally {
      this.events.off('state', onState);
    }
  }
}
