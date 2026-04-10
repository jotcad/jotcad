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
    if (this.maxListeners > 0 && this.listeners[event].length > this.maxListeners) {
        console.error(`MaxListenersExceededWarning: Possible EventEmitter memory leak detected. ${this.listeners[event].length} ${event} listeners added. Use setMaxListeners() to increase limit`);
    }
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
    
    // Normalize nested selectors
    if (obj.path && obj.parameters) return normalizeSelector(obj.path, obj.parameters);

    const result = {};
    for (const [k, v] of Object.entries(obj)) result[k] = clean(v);
    return result;
  };

  return { path, parameters: clean(params) };
};

/**
 * Logical equality check for two selectors.
 */
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
        // Node.js Readable stream
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
 * Core VFS Blackboard logic.
 * Handles state transitions and coordinate-based lookups.
 */
export class VFS {
  constructor(options = {}) {
    this.id = options.id || Math.random().toString(36).slice(2);
    this.getCID = options.getCID;
    this.states = new Map(); // CID -> info
    this.storage = options.storage || new MemoryStorage();
    this.events = new EventEmitter();
    this.events.setMaxListeners(100);
    this.activeReads = new Map();
    this.peers = new Set();
    this.closed = false;
  }

  async close() {
    if (this.closed) return;
    console.log(`[VFS ${this.id}] Closing...`);
    this.closed = true;
    this.events.emit('state', { state: 'CLOSED' });
    this.events.removeAllListeners();
    await this.storage.close();
    console.log(`[VFS ${this.id}] Storage closed.`);
  }

  _checkClosed() {
    if (this.closed) throw new VFSClosedError();
  }

  /**
   * Connects this VFS instance to a peer for state sharing.
   */
  connect(other) {
    this._checkClosed();
    this.peers.add(other);

    // Sync current state to new peer
    for (const [cid, info] of this.states.entries()) {
      const event = {
        cid,
        path: info.path,
        parameters: info.parameters,
        state: info.state,
        source: this.id,
        target: info.target,
      };
      if (typeof other.receive === 'function') {
        other.receive(event, this);
      } else if (typeof other.send === 'function') {
        other.send(event);
      }
    }

    // Bidirectional coupling for local peers
    if (typeof other.receive === 'function' && !other.peers?.has(this)) {
      if (other.connect) other.connect(this);
      else other.peers?.add(this);
    }
  }

  /**
   * Internal emit for state changes.
   */
  async _emit(event) {
    if (this.closed) return;
    if (!event.source) event.source = this.id;
    
    // Ensure CID is present in the event
    if (!event.cid) {
        event.cid = await this.getCID({ path: event.path, parameters: event.parameters });
    }

    this.events.emit('state', event);

    // Propagate to peers
    if (event.source === this.id) {
      for (const peer of this.peers) {
        if (typeof peer.receive === 'function') {
          peer.receive(event, this);
        } else if (typeof peer.send === 'function') {
          peer.send(event);
        }
      }
    }
  }

  /**
   * Externally apply a state change (e.g. from a remote peer).
   */
  async receive(event, from) {
    if (this.closed) return;
    let { cid, state, path, parameters, source, data, target } = event;
    if (source === this.id) return;

    if (!cid) {
        cid = await this.getCID({ path, parameters });
    }

    // console.log(`[VFS ${this.id}] receive: ${path} (${state}) from: ${source}`);

    let info = this.states.get(cid);
    if (!info) {
      info = { path, parameters: parameters || {}, sources: [] };
      this.states.set(cid, info);
    } else {
        // Diagnostic: If we found info, but the path is different, we have a problem
        if (info.path !== path) {
            console.error(`[VFS ${this.id}] CID COLLISION OR MISMAP! CID ${cid} already has path ${info.path}, but received ${path}`);
        }
    }

    if (source && info) {
        if (!info.sources) info.sources = [];
        if (typeof info.sources.includes === 'function' && !info.sources.includes(source)) {
            info.sources.push(source);
        }
    }
    const statesPriority = {
        'LISTENING': 0,
        'PENDING': 1,
        'LINKED': 1,
        'PROVISIONING': 2,
        'AVAILABLE': 3,
        'SCHEMA': 4
    };
    const currentPriority = statesPriority[info.state] || 0;
    const incomingPriority = statesPriority[state] || 0;

    if (incomingPriority >= currentPriority || !info.state) {
        info.state = state;
    }
    
    if (state === 'AVAILABLE' || state === 'SCHEMA') {
      const alreadyHas = await this.storage.has(cid);
      if (!alreadyHas) {
        if (data) {
          // Handle various byte formats (raw Uint8Array, ArrayBuffer, or serialized Node Buffer object)
          let bytes;
          if (data instanceof Uint8Array || data instanceof ArrayBuffer) {
            bytes = data;
          } else if (data && data.type === 'Buffer' && Array.isArray(data.data)) {
            bytes = new Uint8Array(data.data);
          } else if (data && typeof data === 'object') {
            // Check for JSON-serialized Uint8Array format: {"0": 1, "1": 2, ...}
            const keys = Object.keys(data);
            if (keys.length > 0 && keys.every((k) => !isNaN(k))) {
              bytes = new Uint8Array(Object.values(data));
            } else {
              // If it's a plain object but not a known byte format, stringify it
              bytes = new TextEncoder().encode(JSON.stringify(data));
            }
          } else if (typeof data === 'string') {
            bytes = new TextEncoder().encode(data);
          } else {
            bytes = data;
          }

          const stream = new ReadableStream({
            start(controller) {
              controller.enqueue(bytes);
              controller.close();
            },
          });
          await this.storage.set(cid, stream, info);
        }
      }
    }
 else if (state === 'LINKED') {
      info.target = target;
    }

    this.events.emit('state', event);

    // Relay to other peers
    for (const peer of this.peers) {
      if (peer === from) continue;
      if (typeof peer.receive === 'function') {
        peer.receive(event, this);
      } else if (typeof peer.send === 'function') {
        peer.send(event);
      }
    }
  }

  async status(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    const info = this.states.get(cid);
    return info ? info.state : 'MISSING';
  }

  async tickle(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    console.log(`[VFS ${this.id}] tickle(${s.path})`);

    let info = this.states.get(cid);

    if (!info) {
      info = { state: 'PENDING', path: s.path, parameters: s.parameters, sources: [] };
      this.states.set(cid, info);
      await this._emit(
{
        cid,
        path: s.path,
        parameters: info.parameters,
        state: 'PENDING',
      });
    }
    return info.state;
  }

  async read(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    
    // Check for active logical reads
    for (const [activeSelector, promise] of this.activeReads.entries()) {
        if (isMatch(activeSelector, s)) return promise;
    }

    const readPromise = (async () => {
      const state = await this.status(s.path, s.parameters);
      const cid = await this.getCID(s);

      if (state === 'AVAILABLE' || state === 'SCHEMA') {
        const stream = await this.storage.get(cid);
        if (stream) return stream;
      }

      // Signal demand
      await this.receive({
        path: s.path,
        parameters: s.parameters,
        state: 'PENDING',
        source: 'local'
      });

      return new Promise((resolve, reject) => {
        const onState = (event) => {
          if (isMatch(event, s)) {
            if (event.state === 'AVAILABLE' || event.state === 'SCHEMA') {
              this.events.off('state', onState);
              this.activeReads.delete(s);
              // Recursively call read to handle the final retrieval
              this.read(s).then(resolve).catch(reject);
            } else if (event.state === 'LINKED') {
              this.events.off('state', onState);
              this.activeReads.delete(s);
              this.read(event.target).then(resolve).catch(reject);
            }
          }
        };
        this.events.on('state', onState);
      });
    })();

    this.activeReads.set(s, readPromise);
    return readPromise;
  }

  /**
   * Reads data and automatically deserializes it based on the path's schema.
   */
  async readData(pathOrSelector, parameters) {
    const stream = await this.read(pathOrSelector, parameters);
    const s = normalizeSelector(pathOrSelector, parameters);

    // 1. Collect all bytes (Robust consumer for both Web and Node streams)
    const chunks = [];
    if (typeof stream.getReader === 'function') {
        const reader = stream.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
        }
    } else {
        // Node.js Readable stream
        for await (const chunk of stream) {
            chunks.push(chunk);
        }
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

    // 2. Check for schema hint
    const schemaCid = await this.getCID({
      path: s.path + '@schema',
      parameters: {},
    });
    const schemaInfo = this.states.get(schemaCid);

    if (schemaInfo && schemaInfo.state === 'SCHEMA') {
      const schemaStream = await this.storage.get(schemaCid);
      if (schemaStream) {
        const schemaChunks = [];
        if (typeof schemaStream.getReader === 'function') {
            const reader = schemaStream.getReader();
            while (true) {
              const { done, value } = await reader.read();
              if (done) break;
              schemaChunks.push(value);
            }
        } else {
            for await (const chunk of schemaStream) schemaChunks.push(chunk);
        }
        
        let schemaText = '';
        const decoder = new TextDecoder();
        for (const chunk of schemaChunks) schemaText += decoder.decode(chunk, { stream: true });
        schemaText += decoder.decode();

        try {
          if (schemaText.trim()) {
            const schema = JSON.parse(schemaText);
            if (schema.type === 'object' || schema.type === 'array') {
              return JSON.parse(new TextDecoder().decode(bytes));
            }
            if (schema.type === 'mesh') {
              return new TextDecoder().decode(bytes);
            }
          }
        } catch (e) {
            // ... (rest preserved)
          console.warn(`[VFS] Failed to parse schema for ${s.path}`, e);
        }
      }
    }

    // Default heuristics
    const text = new TextDecoder().decode(bytes);
    try {
      // Try JSON first
      if (text.trim().startsWith('{') || text.trim().startsWith('[')) {
        return JSON.parse(text);
      }
    } catch (e) {}

    // Fallback to string if it looks like text, else bytes
    if (/[\x00-\x08\x0E-\x1F]/.test(text)) {
      return bytes;
    }
    return text;
  }

  /**
   * Writes data and automatically serializes it.
   */
  async writeData(pathOrSelector, parameters, data) {
    let bytes;
    if (data instanceof Uint8Array) {
      bytes = data;
    } else if (typeof data === 'string') {
      bytes = new TextEncoder().encode(data);
    } else if (data instanceof ArrayBuffer) {
      bytes = new Uint8Array(data);
    } else {
      // Assume JSON for objects/arrays
      bytes = new TextEncoder().encode(JSON.stringify(data, null, 2));
    }

    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      },
    });

    return this.write(pathOrSelector, parameters, stream);
  }

  /**
   * Links a source coordinate to a target coordinate.
   * Used for parameter normalization.
   */
  async link(sourceSelector, targetSelector) {
    this._checkClosed();
    const src = normalizeSelector(sourceSelector);
    const tgt = normalizeSelector(targetSelector);
    const cid = await this.getCID(src);

    let info = this.states.get(cid);
    if (!info) {
      info = { path: src.path, parameters: src.parameters, sources: [] };
      this.states.set(cid, info);
    }

    info.state = 'LINKED';
    info.target = tgt;

    await this._emit(
{
      cid,
      path: info.path,
      parameters: info.parameters,
      state: 'LINKED',
      target: tgt,
    });
  }

  async lease(pathOrSelector, parameters, duration) {
    this._checkClosed();
    let s, d;
    if (typeof parameters === 'number' && duration === undefined) {
      s = normalizeSelector(pathOrSelector);
      d = parameters;
    } else {
      s = normalizeSelector(pathOrSelector, parameters);
      d = duration;
    }
    const cid = await this.getCID(s);
    let info = this.states.get(cid);
    if (!info || info.state === 'AVAILABLE' || info.state === 'PROVISIONING')
      return false;

    info.state = 'PROVISIONING';
    const leaseId = setTimeout(async () => {
      if (!this.closed && info.state === 'PROVISIONING') {
        info.state = 'PENDING';
        await this._emit({
          cid,
          path: info.path,
          parameters: info.parameters,
          state: 'PENDING',
        });
      }
    }, d);
    info.activeLease = leaseId;
    await this._emit(
{
      cid,
      path: info.path,
      parameters: info.parameters,
      state: 'PROVISIONING',
    });
    return true;
  }

  /**
   * Declares a schema for a path.
   * The schema is stored at a special sub-path and marked with SCHEMA state.
   */
  async declare(path, schema) {
    this._checkClosed();
    const schemaPath = `${path}@schema`;
    const s = normalizeSelector(schemaPath, {});
    const cid = await this.getCID(s);

    let info = this.states.get(cid);
    if (!info) {
      info = { path: s.path, parameters: s.parameters, sources: [] };
      this.states.set(cid, info);
    }

    info.state = 'SCHEMA';
    const schemaText = JSON.stringify(schema, null, 2);
    
    // Store schema as data
    const bytes = new TextEncoder().encode(schemaText);
    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      }
    });
    // Ensure data is in storage BEFORE we emit the state change
    await this.storage.set(cid, stream, info);

    await this._emit({
      cid,
      path: s.path,
      parameters: s.parameters,
      state: 'SCHEMA',
      data: bytes
    });
  }

  async write(pathOrSelector, parameters, stream) {
    this._checkClosed();
    let s, str;
    const isStream =
      parameters &&
      (parameters.on ||
        parameters instanceof ReadableStream ||
        typeof parameters.getReader === 'function' ||
        typeof parameters.pipe === 'function');
    if (isStream) {
      s = normalizeSelector(pathOrSelector);
      str = parameters;
    } else {
      s = normalizeSelector(pathOrSelector, parameters);
      str = stream;
    }
    const cid = await this.getCID(s);
    let info = this.states.get(cid);
    if (!info) {
      info = { path: s.path, parameters: s.parameters, sources: [] };
      this.states.set(cid, info);
    }
    if (info.activeLease) {
      clearTimeout(info.activeLease);
      info.activeLease = null;
    }
    info.state = 'AVAILABLE';

    let relayedData = null;
    if (str instanceof ReadableStream || typeof str.getReader === 'function') {
      const [s1, s2] = str.tee();
      str = s1;
      const reader = s2.getReader();
      const chunks = [];
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }
      const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
      relayedData = new Uint8Array(totalLength);
      let offset = 0;
      for (const chunk of chunks) {
        relayedData.set(chunk, offset);
        offset += chunk.length;
      }
    } else if (str.on || typeof str.pipe === 'function') {
      const chunks = [];
      for await (const chunk of str) {
        if (typeof chunk === 'string') {
            chunks.push(new TextEncoder().encode(chunk));
        } else {
            chunks.push(chunk);
        }
      }
      
      const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
      relayedData = new Uint8Array(totalLength);
      let offset = 0;
      for (const chunk of chunks) {
        relayedData.set(chunk, offset);
        offset += chunk.length;
      }

      str = new ReadableStream({
        start(c) {
          c.enqueue(relayedData);
          c.close();
        },
      });
    }

    await this.storage.set(cid, str, info);
    await this._emit(
{
      cid,
      path: info.path,
      parameters: info.parameters,
      state: 'AVAILABLE',
      data: relayedData,
    });
  }

  async *watch(selector, options = {}) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    const states = options.states || [];
    const signal = options.signal;
    const queue = [];
    let resolveQueue;

    const onState = (event) => {
      if (event.state === 'CLOSED') {
        queue.push(event);
        if (resolveQueue) {
          resolveQueue();
          resolveQueue = null;
        }
        return;
      }
      let pathMatch =
        s.path === '*' ||
        (s.path.endsWith('*')
          ? event.path.startsWith(s.path.slice(0, -1))
          : event.path === s.path);
      if (pathMatch) {
        let paramsMatch = true;
        if (s.parameters && Object.keys(s.parameters).length > 0) {
          for (const [k, v] of Object.entries(s.parameters)) {
            if (event.parameters[k] !== v) {
              paramsMatch = false;
              break;
            }
          }
        }
        if (
          paramsMatch &&
          (states.length === 0 || states.includes(event.state))
        ) {
          queue.push(event);
          if (resolveQueue) {
            resolveQueue();
            resolveQueue = null;
          }
        }
      }
    };

    this.events.on('state', onState);

    const onAbort = () => {
      if (resolveQueue) {
        resolveQueue();
        resolveQueue = null;
      }
    };
    if (signal) signal.addEventListener('abort', onAbort);

    try {
      while (!this.closed && (!signal || !signal.aborted)) {
        if (queue.length === 0)
          await new Promise((resolve) => (resolveQueue = resolve));
        if (this.closed || (signal && signal.aborted)) break;
        while (queue.length > 0) {
          const ev = queue.shift();
          if (ev.state === 'CLOSED') return;
          yield ev;
        }
      }
    } finally {
      this.events.off('state', onState);
      if (signal) signal.removeEventListener('abort', onAbort);
    }
  }

  /**
   * Serializes a set of coordinates into ZFS format.
   * @param {Object|Object[]} selectors A single selector or an array of selectors to include.
   */
  async snapshot(selectors) {
    const list = Array.isArray(selectors) ? selectors : [selectors];
    const entries = [];

    for (const sel of list) {
      const s = normalizeSelector(sel);
      const cid = await this.getCID(s);
      const info = this.states.get(cid);
      if (!info) continue;

      // 1. Store Meta
      const meta = JSON.stringify(
        {
          path: info.path,
          parameters: info.parameters,
          state: info.state,
          target: info.target,
        },
        null,
        2
      );
      entries.push({ 
        filename: `vfs/${cid}.meta`, 
        content: Buffer.from(meta),
        info: info 
      });

      // 2. Store Data
      if (info.state === 'AVAILABLE') {
        const stream = await this.storage.get(cid);
        if (stream) {
          const chunks = [];
          for await (const chunk of stream) chunks.push(chunk);
          entries.push({ 
            filename: `vfs/${cid}.data`, 
            content: Buffer.concat(chunks),
            info: info 
          });
        }
      }
    }

    // 3. Serialize to JOT/ZFS framing
    const buffers = [];
    for (const entry of entries) {
      buffers.push(Buffer.from(`\n=${entry.content.length} ${entry.filename}\n`));
      buffers.push(entry.content);
    }
    return Buffer.concat(buffers);
  }
}
