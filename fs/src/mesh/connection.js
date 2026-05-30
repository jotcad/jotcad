import { Selector } from '../cid.js';

export class VFSRequest {
  constructor(op) {
    if (!op) throw new Error('VFSRequest: op is required');
    this.op = op;
    this.stack = [];
    this.expiresAt = 0;
  }
}

export class ReadSelectorRequest extends VFSRequest {
  constructor(selector, options = {}) {
    super('READ_SELECTOR');
    if (!selector) throw new Error('ReadSelectorRequest: selector is required');
    this.selector = Selector.fromObject(selector);
    this.stack = options.stack || [];
    this.expiresAt = options.expiresAt || (Date.now() + 30000);
  }
}

export class ReadCIDRequest extends VFSRequest {
  constructor(cid, options = {}) {
    super('READ_CID');
    if (!cid) throw new Error('ReadCIDRequest: cid is required');
    if (typeof cid !== 'string') throw new Error('ReadCIDRequest: cid must be a string');
    this.cid = cid;
    this.resolutionStack = options.resolutionStack || [];
    this.stack = options.stack || [];
    this.expiresAt = options.expiresAt || (Date.now() + 30000);
  }
}

export class SpyRequest extends VFSRequest {
  constructor(selector, options = {}) {
    super('SPY');
    if (!selector) throw new Error('SpyRequest: selector is required');
    this.selector = Selector.fromObject(selector);
    this.resolutionStack = options.resolutionStack || [];
    this.stack = options.stack || [];
    this.expiresAt = options.expiresAt || (Date.now() + 30000);
  }
}

export class SubscribeRequest extends VFSRequest {
  constructor(selector, expiresAt, stack = []) {
    super('SUB');
    if (!selector) throw new Error('SubscribeRequest: selector is required');
    this.selector = Selector.fromObject(selector);
    this.expiresAt = expiresAt || (Date.now() + 30000);
    this.stack = stack;
  }
}

export class NotifyRequest extends VFSRequest {
  constructor(selector, payload, stack = []) {
    super('PUB');
    if (!selector) throw new Error('NotifyRequest: selector is required');
    if (payload === undefined) throw new Error('NotifyRequest: payload is required');
    this.selector = Selector.fromObject(selector);
    this.payload = payload;
    this.stack = stack;
  }
}

export class VFSResult {
  constructor(stream, metadata = {}) {
    this.stream = stream; // ReadableStream
    this.metadata = metadata;
  }

  get body() {
    return this.stream;
  }

  get headers() {
    const meta = this.metadata;
    return {
      get(name) {
        if (name === 'x-vfs-info') {
          return typeof meta === 'string' ? meta : JSON.stringify(meta);
        }
        return meta[name] || null;
      }
    };
  }
}

/**
 * Connection: Abstract interface for a communication pipe to a neighbor.
 * Uses a single, unified `send` method delivering strongly-typed request subclasses.
 */
export class Connection {
  constructor(neighborId) {
    this.neighborId = neighborId;
    this.reachability = 'UNKNOWN';
    this.lastPulse = 0;
    this.pps = 0;
    this._pulseCount = 0;
  }

  _tickPPS() {
    this.pps = this._pulseCount;
    this._pulseCount = 0;
  }

  getProtocol() {
    return 'unknown';
  }

  /**
   * Dispatch an encapsulated request to the peer.
   * @param {VFSRequest} req
   * @returns {Promise<VFSResult|void>}
   */
  async send(req) {
    throw new Error('Not implemented');
  }
}
