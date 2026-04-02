import { EventEmitter } from 'events';
import crypto from 'crypto';
import fs from 'fs';
import fsPromises from 'fs/promises';
import path from 'path';
import { pipeline } from 'stream/promises';

/**
 * Normalizes arguments into a standard selector object.
 */
const normalizeSelector = (pathOrSelector, parameters) => {
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

/**
 * Generates a unique Content-ID (CID) for a selector.
 */
const getCID = (selector) => {
  const { path, parameters = {} } = selector;
  if (!path) throw new Error("Selector must have a path");
  const hash = crypto.createHash('sha256');
  hash.update(path);
  const sortedParams = Object.keys(parameters)
    .sort()
    .reduce((acc, key) => {
      acc[key] = parameters[key];
      return acc;
    }, {});
  hash.update(JSON.stringify(sortedParams));
  return hash.digest('hex');
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

export class DiskStorage {
  constructor(root) {
    this.root = root;
    if (!fs.existsSync(this.root)) {
      fs.mkdirSync(this.root, { recursive: true });
    }
  }
  async get(cid) {
    const dataFile = path.join(this.root, `${cid}.data`);
    if (!fs.existsSync(dataFile)) return null;
    return fs.createReadStream(dataFile);
  }
  async set(cid, stream, info) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);
    await fsPromises.writeFile(metaFile, JSON.stringify(info, null, 2));
    const out = fs.createWriteStream(dataFile);
    await pipeline(stream, out);
  }
  async has(cid) {
    return fs.existsSync(path.join(this.root, `${cid}.data`));
  }
  async delete(cid) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);
    if (fs.existsSync(metaFile)) await fsPromises.unlink(metaFile);
    if (fs.existsSync(dataFile)) await fsPromises.unlink(dataFile);
  }
  async close() {}
}

export class VFS {
  constructor(options = {}) {
    this.id = crypto.randomUUID();
    this.states = new Map();
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

  connect(other) {
    this._checkClosed();
    this.peers.add(other);
    other.peers.add(this);
  }

  _emit(event) {
    if (this.closed) return;
    if (!event.source) event.source = this.id;
    this.events.emit('state', event);
    if (event.source === this.id) {
      for (const peer of this.peers) {
        peer.receive(event);
      }
    }
  }

  async receive(event) {
    if (this.closed) return;
    const { cid, state, path, parameters } = event;
    let info = this.states.get(cid);
    if (!info) {
      info = { path, parameters: parameters || {} };
      this.states.set(cid, info);
    }
    info.state = state;
    if (state === 'AVAILABLE' && event.stream) {
      await this.storage.set(cid, event.stream, info);
    }
    this.events.emit('state', event);
  }

  async status(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = getCID(s);
    const info = this.states.get(cid);
    return info ? info.state : 'MISSING';
  }

  async tickle(pathOrSelector, parameters) {
    this._checkClosed();
    const s = normalizeSelector(pathOrSelector, parameters);
    const cid = getCID(s);
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
    const cid = getCID(s);
    await this.tickle(s);

    let info = this.states.get(cid);
    if (info.state === 'AVAILABLE') return this.storage.get(cid);

    return new Promise((resolve, reject) => {
      const onState = (event) => {
        if (event.state === 'CLOSED') {
          this.events.off('state', onState);
          reject(new VFSClosedError());
        } else if (event.cid === cid && event.state === 'AVAILABLE') {
          this.events.off('state', onState);
          resolve(this.storage.get(cid));
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
    const cid = getCID(s);
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
    if (parameters && (parameters.on || parameters instanceof ReadableStream)) {
      s = normalizeSelector(pathOrSelector);
      str = parameters;
    } else {
      s = normalizeSelector(pathOrSelector, parameters);
      str = stream;
    }
    const cid = getCID(s);
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
    await this.storage.set(cid, str, info);
    this._emit({ cid, path: info.path, parameters: info.parameters, state: 'AVAILABLE', stream: str });
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
        if (s.parameters) {
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
