/**
 * Simple EventEmitter shim for both Node and Browser.
 */
class EventEmitter {
  constructor() {
    this.listeners = {};
  }
  on(event, listener) {
    if (!this.listeners[event]) this.listeners[event] = [];
    this.listeners[event].push(listener);
  }
  off(event, listener) {
    if (!this.listeners[event]) return;
    this.listeners[event] = this.listeners[event].filter(l => l !== listener);
  }
  emit(event, ...args) {
    if (!this.listeners[event]) return;
    this.listeners[event].forEach(l => l(...args));
  }
  removeAllListeners() {
    this.listeners = {};
  }
}

/**
 * Normalizes arguments into a standard selector object.
 */
export const normalizeSelector = (pathOrSelector, parameters) => {
  if (typeof pathOrSelector === 'string') {
    return { path: pathOrSelector, parameters: parameters || {} };
  }
  if (pathOrSelector && typeof pathOrSelector === 'object') {
    return {
      path: pathOrSelector.path,
      parameters: pathOrSelector.parameters || parameters || {}
    };
  }
  throw new Error(`Invalid selector: ${pathOrSelector}`);
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
    return entry ? entry.stream : null;
  }
  async set(cid, stream, info) { 
    this.results.set(cid, { info, stream });
  }
  async has(cid) { return this.results.has(cid); }
  async delete(cid) { this.results.delete(cid); }
  async close() { this.results.clear(); }
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
    this.peers = new Set();
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
        source: this.id
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
  _emit(event) {
    if (this.closed) return;
    if (!event.source) event.source = this.id;
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
    const { cid, state, path, parameters, source, data } = event;
    if (source === this.id) return;

    let info = this.states.get(cid);
    if (!info) {
      info = { path, parameters: parameters || {} };
      this.states.set(cid, info);
    }
    
    info.state = state;
    if (state === 'AVAILABLE') {
      const alreadyHas = await this.storage.has(cid);
      if (!alreadyHas) {
        if (data) {
          const bytes = (data instanceof ArrayBuffer) ? new Uint8Array(data) : data;
          const stream = new ReadableStream({
            start(controller) {
              controller.enqueue(bytes);
              controller.close();
            }
          });
          await this.storage.set(cid, stream, info);
        }
      }
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
    let info = this.states.get(cid);
    if (!info) {
      info = { state: 'PENDING', path: s.path, parameters: s.parameters };
      this.states.set(cid, info);
      this._emit({ cid, path: s.path, parameters: info.parameters, state: 'PENDING' });
    }
    return info.state;
  }

  async read(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = await this.getCID(s);
    await this.tickle(s);

    let info = this.states.get(cid);
    if (info.state === 'AVAILABLE') {
        const stream = await this.storage.get(cid);
        if (stream) return stream;
    }

    return new Promise((resolve, reject) => {
      const onState = (event) => {
        if (event.state === 'CLOSED') {
          this.events.off('state', onState);
          reject(new VFSClosedError());
        } else if (event.cid === cid && event.state === 'AVAILABLE') {
          this.events.off('state', onState);
          this.storage.get(cid).then(resolve);
        }
      };
      this.events.on('state', onState);
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
    if (!info || info.state === 'AVAILABLE' || info.state === 'PROVISIONING') return false;

    info.state = 'PROVISIONING';
    const leaseId = setTimeout(() => {
      if (!this.closed && info.state === 'PROVISIONING') {
        info.state = 'PENDING';
        this._emit({ cid, path: info.path, parameters: info.parameters, state: 'PENDING' });
      }
    }, d);
    info.activeLease = leaseId;
    this._emit({ cid, path: info.path, parameters: info.parameters, state: 'PROVISIONING' });
    return true;
  }

  async write(pathOrSelector, parameters, stream) {
    this._checkClosed();
    let s, str;
    const isStream = parameters && (parameters.on || parameters instanceof ReadableStream || typeof parameters.getReader === 'function' || typeof parameters.pipe === 'function');
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
      info = { path: s.path, parameters: s.parameters };
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
        for await (const chunk of str) chunks.push(chunk);
        const buffer = (typeof Buffer !== 'undefined') ? Buffer.concat(chunks) : new Uint8Array(0);
        relayedData = new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
        str = new ReadableStream({
            start(c) { c.enqueue(relayedData); c.close(); }
        });
    }

    await this.storage.set(cid, str, info);
    this._emit({ 
        cid, 
        path: info.path, 
        parameters: info.parameters, 
        state: 'AVAILABLE', 
        data: relayedData 
    });
  }

  async *watch(selector, options = {}) {
    this._checkClosed();
    const s = normalizeSelector(selector);
    const states = options.states || [];
    const queue = [];
    let resolveQueue;

    const onState = (event) => {
      if (event.state === 'CLOSED') {
        queue.push(event);
        if (resolveQueue) { resolveQueue(); resolveQueue = null; }
        return;
      }
      let pathMatch = s.path === '*' || (s.path.endsWith('*') ? event.path.startsWith(s.path.slice(0, -1)) : event.path === s.path);
      if (pathMatch) {
        let paramsMatch = true;
        if (s.parameters && Object.keys(s.parameters).length > 0) {
          for (const [k, v] of Object.entries(s.parameters)) {
            if (event.parameters[k] !== v) { paramsMatch = false; break; }
          }
        }
        if (paramsMatch && (states.length === 0 || states.includes(event.state))) {
          queue.push(event);
          if (resolveQueue) { resolveQueue(); resolveQueue = null; }
        }
      }
    };

    this.events.on('state', onState);
    try {
      while (!this.closed) {
        if (queue.length === 0) await new Promise((resolve) => (resolveQueue = resolve));
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
