import { open as zenohOpen, Config as ZenohConfig, KeyExpr as ZenohKeyExpr, ZBytes as ZenohZBytes, Selector as ZenohSelector, QueryTarget, ConsolidationMode } from '@eclipse-zenoh/zenoh-ts';
import { normalizeSelector, Selector, getSelectorKey } from '../cid.js';
import { log, info, error } from '../log.js';

// VfsRecord helper functions
export function encodeRecord(header, payload = null) {
  const headerStr = JSON.stringify(header);
  const headerBytes = new TextEncoder().encode(headerStr);
  const payloadBytes = payload || new Uint8Array(0);
  const record = new Uint8Array(4 + headerBytes.length + payloadBytes.length);
  const view = new DataView(record.buffer, record.byteOffset, record.byteLength);
  view.setUint32(0, headerBytes.length, false);
  record.set(headerBytes, 4);
  record.set(payloadBytes, 4 + headerBytes.length);
  return record;
}

export function decodeRecord(recordBytes) {
  if (recordBytes.length < 4) return null;
  const view = new DataView(recordBytes.buffer, recordBytes.byteOffset, recordBytes.byteLength);
  const headerLen = view.getUint32(0, false);
  if (recordBytes.length < 4 + headerLen) return null;
  const headerBytes = recordBytes.subarray ? recordBytes.subarray(4, 4 + headerLen) : recordBytes.slice(4, 4 + headerLen);
  const headerStr = new TextDecoder().decode(headerBytes);
  const header = JSON.parse(headerStr);
  const payload = recordBytes.subarray ? recordBytes.subarray(4 + headerLen) : recordBytes.slice(4 + headerLen);
  return { header, payload };
}

// Legacy connection stubs to keep imports from breaking
export class Connection {}
export class ForwardConnection extends Connection {}
export class ReverseConnection extends Connection {}
export class VFSRequest {}
export class VFSResult {}

let isWebSocketWrapped = false;
function wrapWebSocket() {
  if (isWebSocketWrapped || !globalThis.WebSocket) return;
  isWebSocketWrapped = true;
  const OriginalWebSocket = globalThis.WebSocket;
  globalThis.WebSocket = class WrappedWebSocket extends OriginalWebSocket {
    constructor(url, protocols) {
      super(url, protocols);
      // Prevent unhandled 'error' events from crashing the Node.js process (e.g. on DNS lookup failures)
      this.addEventListener('error', () => {});
      const currentMeshLink = MeshLinkBase.activeStartingInstance;
      if (currentMeshLink) {
        this.addEventListener('close', (event) => {
          log(`[MeshLink ${currentMeshLink.vfs.id}] Intercepted WebSocket close event (code ${event.code || 'unknown'}).`);
          currentMeshLink.handleDisconnect().catch(() => {});
        });
      }
    }
  };
}

export class MeshLinkBase {
  static activeStartingInstance = null;
  constructor(vfs, neighborUrls = [], options = {}) {
    wrapWebSocket();
    this.vfs = vfs;
    this.vfs.mesh = this;
    this.instanceId = Math.random().toString(36).slice(2, 8);
    this.options = options;
    this.neighborUrls = neighborUrls || [];
    this.session = null;
    this.queryableCid = null;
    this.queryableOps = new Map();
    this.subscriberNotify = null;
    this.closed = false;
    this.catalog = {};
    this.discoveredProviders = new Set();
    this.reconnectInterval = null;
    this.startLivenessCheck();
  }

  async handleDisconnect() {
    log(`[MeshLink ${this.vfs.id}] Session disconnected. Cleaning up...`);
    if (this.queryableCatalog) {
      try { await this.queryableCatalog.undeclare(); } catch (e) {}
      this.queryableCatalog = null;
    }
    if (this.queryableCid) {
      try { await this.queryableCid.undeclare(); } catch (e) {}
      this.queryableCid = null;
    }
    if (this.queryableOps) {
      for (const queryable of this.queryableOps.values()) {
        try { await queryable.undeclare(); } catch (e) {}
      }
      this.queryableOps.clear();
    }
    if (this.subscriberNotify) {
      try { await this.subscriberNotify.undeclare(); } catch (e) {}
      this.subscriberNotify = null;
    }
    if (this.session) {
      try { await this.session.close(); } catch (e) {}
      this.session = null;
    }
  }

  startLivenessCheck() {
    if (this.reconnectInterval) return;
    this.reconnectInterval = setInterval(async () => {
      if (this.closed) {
        clearInterval(this.reconnectInterval);
        this.reconnectInterval = null;
        return;
      }
      if (this.session) {
        try {
          // Send lightweight heartbeat to verify session health
          await this.session.put(`jot/vfs/ping/${this.vfs.id}`, new Uint8Array([0]));
        } catch (err) {
          log(`[MeshLink ${this.vfs.id}] Connection health check failed: ${err.message}. Reconnecting...`);
          await this.handleDisconnect();
        }
      } else {
        log(`[MeshLink ${this.vfs.id}] Connection is down. Retrying start...`);
        await this.start().catch(() => {});
      }
    }, 5000);
  }


  get connected() {
    return !!this.session;
  }

  async start() {
    if (this.neighborUrls.length === 0) {
      log(`[MeshLink ${this.vfs.id}] No neighbors configured. Operating in standalone mode.`);
      return;
    }

    // Try connecting to first reachable neighbor WS locator
    let connected = false;
    for (const url of this.neighborUrls) {
      const locator = this.toZenohLocator(url);
      log(`[MeshLink ${this.vfs.id}] Attempting connection to Zenoh locator: ${locator}`);
      try {
        const config = new ZenohConfig(locator);
        config.messageResponseTimeoutMs = 600000;
        MeshLinkBase.activeStartingInstance = this;
        this.session = await zenohOpen(config);
        MeshLinkBase.activeStartingInstance = null;
        log(`[MeshLink ${this.vfs.id}] Connected to Zenoh session at ${locator}`);
        connected = true;
        break;
      } catch (err) {
        log(`[MeshLink ${this.vfs.id}] Failed connecting to ${locator}: ${err.message}`);
      }
    }

    if (!connected) {
      log(`[MeshLink ${this.vfs.id}] Warning: Could not connect to any Zenoh neighbors.`);
      return;
    }

    // 1. Declare CID queryable: jot/vfs/cid/**
    try {
      this.queryableCid = await this.session.declareQueryable(`jot/vfs/cid/**`, {
        handler: async (query) => {
          try {
            const key = query.keyExpr().toString();
            const cid = key.slice(12);
            info(`[MeshLink ${this.vfs.id}] Queryable CID: got request for ${cid}`);
            let readResult = await this.vfs.readCID(cid, { localOnly: true }).catch(() => null);
            if (readResult) {
              const { stream, metadata } = readResult;
              info(`[MeshLink ${this.vfs.id}] Queryable CID: Reading stream for ${cid}...`);
              const payload = await this.streamToUint8Array(stream);
              info(`[MeshLink ${this.vfs.id}] Queryable CID: Read stream for ${cid} completed (${payload.length} bytes). Replying...`);
              const header = { status: 200, metadata, encoding: metadata.encoding || 'json' };
              const record = encodeRecord(header, payload);
              await query.reply(key, record);
              info(`[MeshLink ${this.vfs.id}] Queryable CID: Replied to query for ${cid}`);
            } else {
              info(`[MeshLink ${this.vfs.id}] Queryable CID: CID ${cid} not found locally. Replying 404.`);
              const header = { status: 404, error: `CID not found locally: ${cid}` };
              const record = encodeRecord(header);
              await query.reply(key, record);
            }
          } catch (err) {
            info(`[MeshLink ${this.vfs.id}] Error in CID query handler: ${err.message}`);
            try {
              const header = { status: 500, error: err.message };
              const record = encodeRecord(header);
              await query.reply(query.keyExpr().toString(), record);
            } catch (replyErr) {
              info(`[MeshLink ${this.vfs.id}] Double fault: failed to reply error: ${replyErr.message}`);
            }
          } finally {
            info(`[MeshLink ${this.vfs.id}] Queryable CID: Finalizing query.`);
            await query.finalize();
          }
        }
      });
    } catch (err) {
      log(`[MeshLink ${this.vfs.id}] Error declaring CID queryable: ${err.message}`);
    }

    // 1.5 Declare catalog queryable: jot/vfs/catalog
    try {
      this.queryableCatalog = await this.session.declareQueryable(`jot/vfs/catalog`, {
        handler: async (query) => {
          try {
            const key = query.keyExpr().toString();
            log(`[MeshLink ${this.vfs.id}] Queryable catalog: replying with local catalog`);
            const payload = new TextEncoder().encode(JSON.stringify(this.vfs.getCatalog()));
            const header = { status: 200, metadata: { state: 'AVAILABLE', encoding: 'json' }, encoding: 'json' };
            const record = encodeRecord(header, payload);
            await query.reply(key, record);
          } catch (err) {
            log(`[MeshLink ${this.vfs.id}] Error in catalog query handler: ${err.message}`);
          } finally {
            await query.finalize();
          }
        }
      });
    } catch (err) {
      log(`[MeshLink ${this.vfs.id}] Error declaring catalog queryable: ${err.message}`);
    }

    // 2. Declare Operator queryables for all registered providers
    for (const pattern of this.vfs.providers.keys()) {
      await this.registerOp(pattern);
    }

    // 3. Declare notification subscriber: jot/vfs/pub/**
    try {
      this.subscriberNotify = await this.session.declareSubscriber(`*/jot/vfs/pub/**`, {
        handler: (sample) => {
          try {
            const key = sample.keyexpr().toString();
            const pubIdx = key.indexOf('/jot/vfs/pub/');
            const path = pubIdx !== -1 ? key.slice(pubIdx + 13) : key;
            const payloadBytes = sample.payload().toBytes();
            let payload;
            try {
              const payloadStr = new TextDecoder().decode(payloadBytes);
              payload = JSON.parse(payloadStr);
            } catch (err) {
              payload = payloadBytes;
            }
            if (path === 'sys/schema') {
              this.updateCatalog(payload);
            } else {
              this.notify(new Selector(path), payload, ['incoming']);
            }
          } catch (err) {
            console.error(`[MeshLink ${this.vfs.id}] Error in notification subscriber: ${err.stack || err.message}`);
          }
        }
      });
    } catch (err) {
      console.error(`[MeshLink ${this.vfs.id}] Error declaring subscriber: ${err.stack || err.message}`);
    }
    
    // Initial catalog query
    this.queryCatalog();
  }

  toZenohLocator(url) {
    if (url.startsWith('http://') || url.startsWith('https://')) {
      const parsed = new URL(url);
      const port = parseInt(parsed.port, 10);
      const wsPort = port + 1000;
      const isLocal = parsed.hostname === 'localhost' || parsed.hostname === '127.0.0.1';
      const protocol = (url.startsWith('https://') && !isLocal) ? 'wss' : 'ws';
      return `${protocol}://${parsed.hostname}:${wsPort}`;
    }
    if (url.startsWith('ws://') || url.startsWith('wss://')) {
      return url;
    }
    return url;
  }

  async streamToUint8Array(stream) {
    if (stream instanceof Uint8Array) return stream;
    if (stream instanceof ArrayBuffer) return new Uint8Array(stream);
    if (typeof Buffer !== 'undefined' && Buffer.isBuffer(stream)) return new Uint8Array(stream);
    if (stream && typeof stream.getReader === 'function') {
      const reader = stream.getReader();
      const chunks = [];
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value) chunks.push(value);
        }
      } finally {
        reader.releaseLock();
      }
      const len = chunks.reduce((acc, c) => acc + c.length, 0);
      const bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
      return bytes;
    }
    throw new Error('Unsupported stream type in streamToUint8Array');
  }

  async readSelector(selector, context = {}) {
    if (!this.session) return null;
    const { expiresAt = Date.now() + 10000 } = context;

    if (Date.now() > expiresAt) {
      throw new Error(`[MeshLink ${this.vfs.id}] Request expired (expiresAt: ${expiresAt})`);
    }

    const path = selector.path;
    const key = `*/jot/vfs/op/${path}`;
    
    const params = [];
    if (selector.parameters) {
      for (const [k, v] of Object.entries(selector.parameters)) {
        const valStr = (typeof v === 'string') ? v : JSON.stringify(v);
        params.push(`${k}=${encodeURIComponent(valStr)}`);
      }
    }
    if (selector.output) {
      params.push(`output=${encodeURIComponent(selector.output)}`);
    }
    const queryExpr = params.length > 0 ? `${key}?${params.join(';')}` : key;
    
    log(`[MeshLink ${this.vfs.id}] readSelector generative: z_get(${queryExpr})`);
    
    try {
      const receiver = await this.session.get(queryExpr, {
        timeout: expiresAt - Date.now() > 0 ? expiresAt - Date.now() : 10000,
        target: 1
      });
      if (receiver) {
        for await (const reply of receiver) {
          const result = reply.result();
          if (result instanceof Error || result.constructor.name === 'ReplyError') {
            throw new Error(result.payload ? result.payload().toString() : result.message);
          }
          
          const sample = result;
          const recordBytes = sample.payload().toBytes();
          const decoded = decodeRecord(recordBytes);
          if (!decoded) throw new Error("Failed to decode VfsRecord");
          
          const { header, payload } = decoded;
          if (header.status !== 200) {
            if (header.status === 503 || header.status === 429) {
              log(`[MeshLink ${this.vfs.id}] Target returned ${header.status} (Busy). Spilling over to next reply...`);
              continue;
            }
            throw new Error(header.error || `Remote server returned status ${header.status}`);
          }
          
          const stream = new ReadableStream({
            start(controller) {
              controller.enqueue(payload);
              controller.close();
            }
          });
          return { stream, metadata: header.metadata };
        }
      }
    } catch (err) {
      error(`[MeshLink ${this.vfs.id}] readSelector generative query failed: ${err.message}`);
      throw err;
    }

    return null;
  }

  async readCID(cid, context = {}) {
    if (!this.session) return null;
    const expiresAt = Math.min(context.expiresAt || Infinity, Date.now() + 2000);
    
    const key = `jot/vfs/cid/${cid}`;
    log(`[MeshLink ${this.vfs.id}] readCID: z_get(${key})`);
    
    try {
      const receiver = await this.session.get(key, {
        timeout: expiresAt - Date.now() > 0 ? expiresAt - Date.now() : 10000,
        target: QueryTarget.ALL,
        consolidation: ConsolidationMode.NONE
      });
      if (!receiver) throw new Error("No receiver returned");
      
      for await (const reply of receiver) {
        const result = reply.result();
        if (result instanceof Error || result.constructor.name === 'ReplyError') {
          throw new Error(result.payload ? result.payload().toString() : result.message);
        }
        
        const sample = result;
        const recordBytes = sample.payload().toBytes();
        const decoded = decodeRecord(recordBytes);
        if (!decoded) throw new Error("Failed to decode VfsRecord");
        
        const { header, payload } = decoded;
        log(`[MeshLink ${this.vfs.id}] readCID got reply status: ${header.status} for CID: ${cid}`);
        if (header.status !== 200) {
          if (header.status === 404) continue;
          throw new Error(header.error || `Remote server returned status ${header.status}`);
        }
        
        const stream = new ReadableStream({
          start(controller) {
            controller.enqueue(payload);
            controller.close();
          }
        });
        return { stream, metadata: header.metadata };
      }
    } catch (err) {
      error(`[MeshLink ${this.vfs.id}] readCID failed: ${err.message}`);
      throw err;
    }
    return null;
  }

  async queryCatalog() {
    if (!this.session) return;
    log(`[MeshLink ${this.vfs.id}] Initial catalog query on Zenoh mesh...`);
    try {
      const receiver = await this.session.get('jot/vfs/catalog', {
        timeout: 5000,
        target: QueryTarget.ALL,
        consolidation: 1
      });
      if (receiver) {
        for await (const reply of receiver) {
          const result = reply.result();
          if (result instanceof Error || result.constructor.name === 'ReplyError') continue;
          
          const sample = result;
          const recordBytes = sample.payload().toBytes();
          const decoded = decodeRecord(recordBytes);
          if (!decoded) continue;
          
          const { header, payload } = decoded;
          if (header.status === 200) {
            const catalogPayload = JSON.parse(new TextDecoder().decode(payload));
            if (catalogPayload.provider !== this.vfs.id) {
              this.updateCatalog(catalogPayload);
            }
          }
        }
      }
    } catch (err) {
      log(`[MeshLink ${this.vfs.id}] Catalog query failed: ${err.message}`);
    }
  }

  updateCatalog(payload) {
    if (!payload || !payload.catalog) return;
    this.catalog = this.catalog || {};
    this.discoveredProviders = this.discoveredProviders || new Set();
    
    this.discoveredProviders.add(payload.provider);
    
    let changed = false;
    for (const [name, schema] of Object.entries(payload.catalog)) {
      if (!this.catalog[name]) {
        this.catalog[name] = schema;
        changed = true;
      }
    }
    
    if (changed) {
      log(`[MeshLink ${this.vfs.id}] Catalog updated from ${payload.provider}, now has ${Object.keys(this.catalog).length} operators.`);
      const unifiedPayload = {
        provider: payload.provider || 'mesh-union',
        catalog: this.catalog
      };
      this.notify(new Selector('sys/schema'), unifiedPayload, ['incoming']);
    }
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    log(`[MeshLink ${this.vfs.id}] subscribe interest registered: ${selector.path}`);
    if (selector.path === 'sys/schema') {
      if (this.catalog && Object.keys(this.catalog).length > 0) {
        const primaryProvider = [...this.discoveredProviders][0] || 'mesh-union';
        const unifiedPayload = {
          provider: primaryProvider,
          catalog: this.catalog
        };
        setTimeout(() => {
          this.notify(new Selector('sys/schema'), unifiedPayload, ['incoming']);
        }, 0);
      } else {
        this.queryCatalog();
      }
    }
  }

  notify(selector, payload, stack = []) {
    if (stack && stack.includes('incoming')) {
      this.vfs.events.emit('notify', selector, payload);
      return;
    }
    if (!this.session) return;
    const key = `${this.vfs.id}/jot/vfs/pub/${selector.path}`;
    log(`[MeshLink ${this.vfs.id}] notify: publishing to ${key}`);
    let bytes;
    if (payload instanceof Uint8Array || (typeof Buffer !== 'undefined' && Buffer.isBuffer(payload))) {
      bytes = payload;
    } else {
      bytes = new TextEncoder().encode(JSON.stringify(payload));
    }
    this.session.put(key, bytes).catch(err => {
      error(`[MeshLink ${this.vfs.id}] notify failed: ${err.message}`);
    });
  }

  // Compatibility stubs for registerVFSRoutes
  addInterest(peerId, selector, expiresAt, stack) {
    this.subscribe(selector, expiresAt, stack);
  }

  async registerPeer(peerId, peerUrl) {
    return { id: this.vfs.id, reachability: 'DIRECT', transports: ['ws'] };
  }

  async registerOp(pattern) {
    if (!this.session) return;
    this.queryableOps = this.queryableOps || new Map();
    if (this.queryableOps.has(pattern)) return;

    const key = `${this.vfs.id}/jot/vfs/op/${pattern}`;
    log(`[MeshLink ${this.vfs.id}] Declaring specific queryable for operator: ${key}`);
    try {
      const queryable = await this.session.declareQueryable(key, {
        handler: async (query) => {
          try {
            const key = query.keyExpr().toString();
            const opIdx = key.indexOf('/jot/vfs/op/');
            const opPath = opIdx !== -1 ? key.slice(opIdx + 12) : key;
            
            const queryParams = new Map();
            for (const [k, v] of query.parameters().iter()) {
              const decoded = decodeURIComponent(v);
              try {
                const parsed = JSON.parse(decoded);
                if (parsed && typeof parsed === 'object' && typeof parsed.path === 'string') {
                  queryParams.set(k, Selector.fromObject(parsed));
                } else {
                  queryParams.set(k, parsed);
                }
              } catch {
                queryParams.set(k, decoded);
              }
            }
            
            let output = "";
            if (queryParams.has("output")) {
              output = queryParams.get("output");
              queryParams.delete("output");
            }
            
            const paramsObj = Object.fromEntries(queryParams.entries());
            let selector = new Selector(opPath, paramsObj);
            if (output) {
              selector = selector.withOutput(output);
            }
            log(`[MeshLink ${this.vfs.id}] Queryable OP (${pattern}): got request for ${selector.path}`);
            
            const result = await this.vfs.readSelector(selector);
            if (result) {
              const { stream, metadata } = result;
              const payload = await this.streamToUint8Array(stream);
              const header = { status: 200, metadata, encoding: metadata.encoding || 'json' };
              const record = encodeRecord(header, payload);
              await query.reply(key, record);
            } else {
              log(`[MeshLink ${this.vfs.id}] Queryable OP (${pattern}): Selector ${selector.path} not found locally. Skipping reply.`);
            }
          } catch (err) {
            log(`[MeshLink ${this.vfs.id}] Error in OP query handler for ${pattern}: ${err.message}`);
            const header = { status: 500, error: err.message };
            const record = encodeRecord(header);
            await query.reply(query.keyExpr().toString(), record);
          } finally {
            await query.finalize();
          }
        }
      });
      this.queryableOps.set(pattern, queryable);
    } catch (err) {
      log(`[MeshLink ${this.vfs.id}] Error declaring specific queryable for ${key}: ${err.message}`);
    }
  }

  async stop() {
    this.closed = true;
    if (this.reconnectInterval) {
      clearInterval(this.reconnectInterval);
      this.reconnectInterval = null;
    }
    if (this.queryableCatalog) {
      try { await this.queryableCatalog.undeclare(); } catch (e) {}
      this.queryableCatalog = null;
    }
    if (this.queryableCid) {
      try { await this.queryableCid.undeclare(); } catch (e) {}
      this.queryableCid = null;
    }
    if (this.queryableOps) {
      for (const queryable of this.queryableOps.values()) {
        try { await queryable.undeclare(); } catch (e) {}
      }
      this.queryableOps.clear();
    }
    if (this.subscriberNotify) {
      try { await this.subscriberNotify.undeclare(); } catch (e) {}
      this.subscriberNotify = null;
    }
    if (this.session) {
      try { await this.session.close(); } catch (e) {}
      this.session = null;
    }
  }
}

export class MeshLink extends MeshLinkBase {}
