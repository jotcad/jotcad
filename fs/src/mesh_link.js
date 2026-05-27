import crypto from 'crypto';
import { normalizeSelector, Selector, decodeSafe, decodeInfo } from './cid.js';
import { log } from './log.js';

/**
 * Connection: Abstract interface for a direct communication pipe to a neighbor.
 */
class Connection {
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

  async read(selector, context) { throw new Error('Not implemented'); }
  async spy(selector, context) { throw new Error('Not implemented'); }
  async subscribe(selector, expiresAt, stack) { throw new Error('Not implemented'); }
  async notify(selector, payload, stack) { throw new Error('Not implemented'); }
}

/**
 * ForwardConnection: A pipe using outgoing HTTP requests to a stable neighbor URL.
 */
export class ForwardConnection extends Connection {
  constructor(neighborId, url, fetch, options = {}) {
    super(neighborId);
    this.url = url.replace(/\/$/, '');
    this.fetch = fetch;
    this.localUrl = options.localUrl;
    this.signal = options.signal;
    this.reachability = options.reachability || 'DIRECT';
  }

  async read(target, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const isSelector = target instanceof Selector;
    
    const headers = { 
        'x-vfs-id': stack[0],
        'x-vfs-op': 'READ',
        'x-vfs-stack': stack.join(','),
        'x-vfs-expires': String(expiresAt)
    };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;
    if (isSelector) headers['x-vfs-selector'] = JSON.stringify(target);

    const body = isSelector ? null : JSON.stringify({ cid: target, resolutionStack });

    try {
        log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/read`, isSelector ? target.path : target);
        const resp = await this.fetch(`${this.url}/read`, {
          method: 'POST',
          headers,
          signal: this.signal || AbortSignal.timeout(5000),
          body,
        });
        log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${this.url}/read`);

        if (resp.ok && resp.body) return { body: resp.body, headers: resp.headers };
        
        const errText = await resp.text().catch(() => 'no body');
        const ctxLine = JSON.stringify({ vfsId: this.neighborId, status: resp.status, action: 'mesh_read', url: this.url });
        throw new Error(ctxLine + '\n' + errText);
    } catch (err) {
        log(`[MeshLink ${this.neighborId}] read error: ${err.message}`);
        throw err;
    }
  }

  async spy(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 
        'Content-Type': 'application/json', 
        'x-vfs-id': stack[0],
        'x-vfs-op': 'SPY',
        'x-vfs-selector': JSON.stringify(selector),
        'x-vfs-stack': stack.join(','),
        'x-vfs-expires': String(expiresAt)
    };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;

    const resp = await this.fetch(`${this.url}/spy`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
      body: JSON.stringify({ resolutionStack }),
    });

    if (resp.ok && resp.body) return resp.body;
    return null;
  }

  async subscribe(selector, expiresAt, stack) {
    const s = normalizeSelector(selector);
    const headers = {
        'x-vfs-op': 'SUB',
        'x-vfs-selector': JSON.stringify(s),
        'x-vfs-expires': String(expiresAt),
        'x-vfs-stack': stack.join(','),
        'x-vfs-id': stack[stack.length - 1]
    };
    log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/subscribe`, s.path);
    await this.fetch(`${this.url}/subscribe`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
    }).then(r => log(`[MeshLink ${this.neighborId}] <- ${r.status} ${this.url}/subscribe`))
      .catch(() => {});
  }

  async notify(selector, payload, stack = []) {
    const s = normalizeSelector(selector);
    this._pulseCount++;
    this.lastPulse = Date.now();

    const isBinary = payload instanceof Uint8Array;
    const headers = {
        'x-vfs-op': 'PUB',
        'x-vfs-selector': JSON.stringify(s),
        'x-vfs-stack': stack.join(','),
        'x-vfs-encoding': isBinary ? 'bytes' : 'json'
    };
    if (!isBinary) headers['Content-Type'] = 'application/json';

    const body = isBinary ? payload : JSON.stringify(payload);

    log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/notify`, s.path, isBinary ? `(BINARY ${payload.length} bytes)` : '');
    await this.fetch(`${this.url}/notify`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
      body,
    }).then(r => log(`[MeshLink ${this.neighborId}] <- ${r.status} ${this.url}/notify`))
      .catch(() => {});
  }
}

/**
 * ReverseConnection: A pipe reached by replying to a neighbor's pending /listen poll.
 * Encapsulates BOTH the server-side pool management and the client-side poll loop.
 */
export class ReverseConnection extends Connection {
  constructor(neighborId, mesh, options = {}) {
    super(neighborId);
    this.mesh = mesh;
    this.vfs = mesh.vfs;
    this.instanceId = options.instanceId || Math.random().toString(36).slice(2, 8);
    this.reachability = 'REVERSE';
    
    // Server-side state
    this.pool = [];
    this.queue = [];
    this.replies = new Map();
    
    // Client-side state
    this.abortController = new AbortController();
  }

  stop() {
    this.abortController.abort();
  }

  /**
   * SERVER SIDE: Entry point for a hidden peer calling /listen.
   */
  addScanner(res, replyTo = null, stream = null, info = null) {
    // Protocol Rule: Permanent Tunnel
    // Prevent Node.js from timing out the socket (default is 2m)
    if (res.setTimeout) res.setTimeout(0);

    // Protocol Rule: Superseding (No more 409)
    // Clear any existing active polls from this peer
    if (this.pool.length > 0) {
        log(`[ReverseConn ${this.neighborId}] Superseding active poll line.`);
        for (const item of this.pool) {
            try {
                item.res.writeHead(204);
                item.res.end();
            } catch (e) {}
        }
        this.pool = [];
    }

    // Handle asynchronous read/spy replies via the tunnel
    if (replyTo) {
      const resolveOrReject = this.replies.get(replyTo);
      if (resolveOrReject) {
        this.replies.delete(replyTo);
        const headers = new Map();
        if (info) headers.set('x-vfs-info', info);
        const decodedInfo = decodeInfo(info);
        if (decodedInfo && decodedInfo.error) {
          resolveOrReject(null, headers, new Error(decodedInfo.error));
        } else {
          resolveOrReject(stream, headers);
        }
      }
    }

    // Flush any pending commands immediately
    if (this.queue.length > 0) {
      const cmd = this.queue.shift();
      log(`[ReverseConn ${this.neighborId}] Delivering queued command ${cmd.op || cmd.type} immediately.`);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify(cmd));
      return;
    }

    // Otherwise, park the response in the pool
    this.pool.push({ res });
    res.on('close', () => {
      const idx = this.pool.findIndex((item) => item.res === res);
      if (idx !== -1) this.pool.splice(idx, 1);
    });
  }

  /**
   * SERVER SIDE: Send a command or notification by consuming a pooled /listen response.
   */
  _dispatch(command) {
    log(`[ReverseConn ${this.neighborId}] Dispatching command ${command.op || command.type}`);
    if (this.pool.length > 0) {
      const { res } = this.pool.shift();
      
      const isBinary = command.payload instanceof Uint8Array;
      const headers = { 
        'Content-Type': isBinary ? 'application/octet-stream' : 'application/json',
        'x-vfs-op': command.op || command.type,
        'x-vfs-encoding': isBinary ? 'bytes' : 'json'
      };
      if (command.selector) headers['x-vfs-selector'] = JSON.stringify(command.selector);
      if (command.stack) headers['x-vfs-stack'] = command.stack.join(',');
      if (command.expiresAt) headers['x-vfs-expires'] = String(command.expiresAt);
      if (command.id) headers['x-vfs-reply-to'] = command.id;

      res.writeHead(200, headers);
      
      let body;
      if (isBinary) {
          body = command.payload;
      } else {
          body = JSON.stringify(command.payload !== undefined ? command.payload : command);
      }
      res.end(body);
      return true;
    }
    this.queue.push(command);
    return false;
  }

  async read(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.replies.delete(requestId);
        reject(new Error(`Reverse read timeout for ${selector.path || selector}`));
      }, Math.max(0, expiresAt - Date.now()));

      this.replies.set(requestId, (stream, headers = new Map(), err = null) => {
        clearTimeout(timeout);
        if (err) reject(err);
        else resolve({ body: stream, headers });
      });
    });

    this._dispatch({ 
        type: 'COMMAND', 
        op: 'READ', 
        id: requestId, 
        selector, 
        stack, 
        resolutionStack: context.resolutionStack || [],
        expiresAt 
    });
    return replyPromise;
  }

  async spy(selector, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const requestId = globalThis.crypto.randomUUID();
    const replyPromise = new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.replies.delete(requestId);
        reject(new Error(`Reverse spy timeout for ${selector.path}`));
      }, expiresAt - Date.now());

      this.replies.set(requestId, (stream, headers = new Map(), err = null) => {
        clearTimeout(timeout);
        if (err) reject(err);
        else resolve({ body: stream, headers });
      });
    });

    this._dispatch({ 
        type: 'COMMAND', 
        op: 'SPY', 
        id: requestId, 
        selector, 
        stack, 
        resolutionStack,
        expiresAt 
    });
    return replyPromise;
  }

  async subscribe(selector, expiresAt, stack) {
    this._dispatch({ type: 'COMMAND', op: 'SUB', selector, expiresAt, stack });
  }

  async notify(selector, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    this._dispatch({ type: 'PUB', selector, payload, stack });
  }

  /**
   * CLIENT SIDE: Actively poll a visible neighbor to receive mesh commands.
   */
  async startPolling(baseUrl, fetch) {
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.fetch = fetch;
    log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} starting Poll-based Receiver for ${baseUrl}`);
    let replyTo = null, stream = null, metadata = null;
    let consecutiveFailures = 0;
    const MAX_FAILURES = 5;
    
    while (!this.abortController.signal.aborted) {
      log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- ENTERING LOOP ITERATION ---`);
      try {
        const headers = { 'x-vfs-peer-id': this.vfs.id };
        if (replyTo) headers['x-vfs-reply-to'] = replyTo;
         if (metadata) {
            const str = JSON.stringify(metadata);
            headers['x-vfs-info'] = typeof Buffer !== 'undefined'
              ? Buffer.from(str).toString('base64')
              : btoa(unescape(encodeURIComponent(str)));
         }
        
        let bodyToSend = stream;
        const fetchOptions = { 
            method: 'POST', 
            headers, 
            signal: this.abortController.signal
        };

        // Browser Compatibility: Streaming bodies REQUIRE HTTP/2 in Chrome.
        // Since our nodes are HTTP/1.1, we must "drain" the stream into a buffer first
        // to use a standard buffered POST.
        if (stream instanceof ReadableStream || (stream && typeof stream.getReader === 'function')) {
            log(`[ReverseConn ${this.neighborId}] Draining stream for HTTP/1.1 compatibility...`);
            const reader = stream.getReader();
            const chunks = [];
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value);
            }
            const totalLen = chunks.reduce((acc, c) => acc + c.length, 0);
            const merged = new Uint8Array(totalLen);
            let offset = 0;
            for (const c of chunks) { merged.set(c, offset); offset += c.length; }
            bodyToSend = merged;
        }

        if (bodyToSend !== undefined) fetchOptions.body = bodyToSend;
        
        log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} -> WAITING for next command from ${baseUrl}`);
        const resp = await fetch(`${baseUrl}/listen`, fetchOptions);
        replyTo = null; stream = null; metadata = null;
        consecutiveFailures = 0; // Successful roundtrip

        log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} <- GOT SOMETHING (status ${resp.status}) from ${baseUrl}`);
        if (resp.status === 200) {
          const h = resp.headers;
          const op = h && typeof h.get === 'function' ? h.get('x-vfs-op') : null;
          const encoding = h && typeof h.get === 'function' ? h.get('x-vfs-encoding') : null;
          const selectorB64 = h && typeof h.get === 'function' ? h.get('x-vfs-selector') : null;
          const stackStr = h && typeof h.get === 'function' ? h.get('x-vfs-stack') : null;
          const expiresStr = h && typeof h.get === 'function' ? h.get('x-vfs-expires') : null;
          const replyToHeader = h && typeof h.get === 'function' ? h.get('x-vfs-reply-to') : null;

          let cmd, payload;
          if (op) {
            let selector = null;
            if (selectorB64) {
                try {
                    selector = Selector.fromObject(JSON.parse(selectorB64));
                } catch (e) {
                    try { selector = decodeSafe(selectorB64); } catch (e2) {}
                }
            }
            const stack = stackStr ? stackStr.split(',').map(s => s.trim()) : [];
            const expiresAt = expiresStr ? parseInt(expiresStr) : null;

            if (encoding === 'bytes') {
                payload = new Uint8Array(await resp.arrayBuffer());
            } else {
                const text = await resp.text();
                try { 
                    const body = JSON.parse(text); 
                    payload = body.payload !== undefined ? body.payload : body;
                } catch (e) { payload = text; }
            }
            cmd = { type: op === 'PUB' ? 'PUB' : 'COMMAND', op, selector, payload, stack, expiresAt, id: replyToHeader };
          } else {
            const cmdText = await resp.text();
            cmd = JSON.parse(cmdText);
          }

          log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} - RECEIVED ${cmd.op || cmd.type}`);

          if (cmd.type === 'COMMAND') {
            const sel = cmd.selector ? Selector.fromObject(cmd.selector) : null;
            if (cmd.op === 'READ') {
              try {
                const result = await this.vfs.read(sel || cmd.cid, { stack: cmd.stack, resolutionStack: cmd.resolutionStack, expiresAt: cmd.expiresAt });
                if (result) {
                    stream = result.stream;
                    metadata = result.metadata;
                }
              } catch (readErr) {
                console.error(`[ReverseConn ${this.neighborId}] Read failed for selector:`, sel || cmd.cid, readErr.message);
                metadata = { error: readErr.message };
                stream = null;
              }
              replyTo = cmd.id;
            } else if (cmd.op === 'SPY') {
              try {
                stream = await this.vfs.spy(sel, { stack: cmd.stack, resolutionStack: cmd.resolutionStack, expiresAt: cmd.expiresAt });
              } catch (spyErr) {
                console.error(`[ReverseConn ${this.neighborId}] Spy failed for selector:`, sel, spyErr.message);
                metadata = { error: spyErr.message };
                stream = null;
              }
              replyTo = cmd.id;
            } else if (cmd.op === 'SUB') {
              try {
                this.mesh.addInterest(cmd.stack[0] || 'unknown', sel, cmd.expiresAt, cmd.stack);
              } catch (subErr) {
                console.error(`[ReverseConn ${this.neighborId}] Sub failed for selector:`, sel, subErr.message);
              }
            }
          } else if (cmd.type === 'PUB') {
            try {
              const s = cmd.selector ? Selector.fromObject(cmd.selector) : null;
              this.mesh.notify(s, cmd.payload, cmd.stack);
            } catch (notifyErr) {
              console.error(`[ReverseConn ${this.neighborId}] Notify failed:`, notifyErr.message);
            }
          }
          log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- FINISHED command. Looping. ---`);
        } else if (resp.status === 409) {
            const errText = await resp.text();
            throw new Error(`CRITICAL: Remote node rejected our poll: ${errText}`);
        } else { 
          if (resp.status !== 204) log(`[ReverseConn ${this.neighborId}] poll non-200 response: ${resp.status}`);
          log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} --- IDLE. Looping. ---`);
        }
      } catch (err) { 
        if (this.abortController.signal.aborted) break; 
        consecutiveFailures++;
         console.error(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} !!! POLL LOOP ERROR !!! (Attempt ${consecutiveFailures}/${MAX_FAILURES}) Failed to fetch endpoint: ${baseUrl}/listen. Error: ${err.message}`);
        if (err.message.includes('CRITICAL:') || consecutiveFailures >= MAX_FAILURES) {
          const detailedErr = new Error(`Failed to fetch endpoint: ${baseUrl}/listen. Error: ${err.message}`);
          detailedErr.stack = err.stack;
          throw detailedErr; // Throw a detailed custom error!
        }
        // Transient network dropout (e.g., Failed to fetch): back off 2s and continue polling
        await new Promise((resolve) => setTimeout(resolve, 2000));
      }
    }
  }

  stop() {
    this.abortController.abort();
    for (const { res } of this.pool) {
      try { res.end(); } catch (e) {}
    }
    this.pool = [];
  }
}

/**
 * MeshLink: Direct Neighbor Registry and Recursive Bread-crumb Router.
 */
export class MeshLinkBase {
  constructor(vfs, neighborUrls = [], options = {}) {
    this.vfs = vfs;
    this.instanceId = Math.random().toString(36).slice(2, 8);
    this.fetch = options.fetch || globalThis.fetch.bind(globalThis);
    this.localUrl = options.localUrl;
    log(`[MeshLink ${this.vfs.id}] Instance ${this.instanceId} created.`);
    this.peers = new Map(); // neighborId -> Connection
    this.connecting = new Set();
    this.pollers = []; // Private client-side long-polling receivers
    this.neighborUrls = neighborUrls.map((u) => u.replace(/\/$/, ''));
    this.abortController = new AbortController();

    this.interests = new Map();

    this._ppsInterval = setInterval(() => {
      for (const conn of this.peers.values()) conn._tickPPS();
    }, 1000);
    if (this._ppsInterval.unref) this._ppsInterval.unref();
  }

  /**
   * INTERNAL: Enforce strictly one Connection per peer.
   */
  _setPeer(id, conn) {
    if (this.peers.has(id)) {
      const existing = this.peers.get(id);
      throw new Error(`CRITICAL PROTOCOL VIOLATION: Peer ${id} already has an active connection (${existing.reachability}). Refusing to create duplicate.`);
    }
    this.peers.set(id, conn);
  }

  async subscribe(selector, expiresAt = Date.now() + 60000, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    
    // Paint the local interest field
    const topic = JSON.stringify(s);
    if (!this.interests.has(topic)) {
        this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null, localExpiresAt: 0 });
    }
    const entry = this.interests.get(topic);
    entry.localExpiresAt = Math.max(entry.localExpiresAt, expiresAt);
    log(`[MeshLink ${this.vfs.id}] Local interest in ${s.path} recorded (expiresAt: ${expiresAt})`);
    
    // Propagate the interest (Painting the mesh)
    const nextStack = [...stack, this.vfs.id];
    for (const conn of this.peers.values()) {
      if (!nextStack.includes(conn.neighborId)) {
        log(`[MeshLink ${this.vfs.id}] -> Propagating local interest in ${s.path} to peer ${conn.neighborId}`);
        conn.subscribe(s, expiresAt, nextStack).catch(() => {});
      }
    }
  }

  addInterest(neighborId, selector, expiresAt, stack = []) {
    const s = normalizeSelector(selector);
    const topic = JSON.stringify(s);
    log(`[MeshLink ${this.vfs.id}] addInterest: ${s.path} from ${neighborId} (stack: ${stack})`);
    if (!this.interests.has(topic)) {
      this.interests.set(topic, { selector: s, subs: new Map(), lastValue: null, localExpiresAt: 0 });
    }
    
    const entry = this.interests.get(topic);
    const isNewNeighbor = !entry.subs.has(neighborId);
    entry.subs.set(neighborId, expiresAt);

    // IMMEDIATE REPLY PHASE: 
    // 1. If we have a cached last value from the mesh, push it.
    if (isNewNeighbor && entry.lastValue) {
      const conn = this.peers.get(neighborId);
      if (conn) {
          log(`[MeshLink ${this.vfs.id}] -> Pushing cached lastValue for ${s.path} to ${neighborId}`);
          conn.notify(s, entry.lastValue, [this.vfs.id]).catch(() => {});
      }
    }

    // 2. If it's sys/schema, we MUST also contribute our own local catalog
    if (isNewNeighbor && s.path === 'sys/schema') {
        const conn = this.peers.get(neighborId);
        if (conn) {
            const catalog = this.vfs.getCatalog();
            log(`[MeshLink ${this.vfs.id}] -> Contributing local catalog to ${neighborId} (${Object.keys(catalog.catalog).length} ops)`);
            conn.notify(s, catalog, [this.vfs.id]).catch(() => {});
        }
    }

    // 3. If it's sys/topo, we MUST also contribute our own local topology update
    if (isNewNeighbor && s.path === 'sys/topo') {
        const conn = this.peers.get(neighborId);
        if (conn) {
            log(`[MeshLink ${this.vfs.id}] -> Contributing local topology to new subscriber ${neighborId}`);
            conn.notify(s, {
                peer: this.vfs.id,
                neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability }))
            }, [this.vfs.id]).catch(() => {});
        }
    }

    // PROPAGATION PHASE:
    if (isNewNeighbor) {
      const nextStack = [...stack, this.vfs.id];
      for (const conn of this.peers.values()) {
        if (!nextStack.includes(conn.neighborId) && conn.neighborId !== neighborId) {
          log(`[MeshLink ${this.vfs.id}] -> Propagating interest in ${s.path} from ${neighborId} to ${conn.neighborId}`);
          conn.subscribe(s, expiresAt, nextStack).catch(() => {});
        }
      }
    }
  }

  notify(selector, payload, stack = []) {
    const s = normalizeSelector(selector);
    if (stack.includes(this.vfs.id)) return;
    const nextStack = [...stack, this.vfs.id];

    log(`[MeshLink ${this.vfs.id}] notify: ${s.path} from stack ${stack}`);

    // Ensure we cache the last value on the node, even if there are no active local subscriptions yet.
    // This solves the timing/bootstrap race where a node broadcasts its initial topology/state
    // before the peer (e.g. browser UX) has completed its startup handshake and subscribed.
    const topic = JSON.stringify(s);
    if (!this.interests.has(topic)) {
      this.interests.set(topic, { selector: s, subs: new Map(), lastValue: payload, localExpiresAt: 0 });
    } else {
      this.interests.get(topic).lastValue = payload;
    }

    // 1. Local Delivery
    for (const entry of this.interests.values()) {
      if (this._matches(entry.selector, s)) {
        entry.lastValue = payload;
        if (entry.localExpiresAt > Date.now()) {
          log(`[MeshLink ${this.vfs.id}] -> Local delivery for ${s.path}`);
          this.vfs.events.emit('notify', s, payload);
        }
      }
    }

    // 2. Neighbor Propagation
    for (const entry of this.interests.values()) {
      if (this._matches(entry.selector, s)) {
        for (const [neighborId, expiry] of entry.subs.entries()) {
          if (Date.now() > expiry) { entry.subs.delete(neighborId); continue; }
          if (stack.includes(neighborId)) continue;
          
          const conn = this.peers.get(neighborId);
          if (conn) {
              log(`[MeshLink ${this.vfs.id}] -> Forwarding notification for ${s.path} to ${neighborId}`);
              conn.notify(s, payload, nextStack).catch(() => {});
          }
        }
      }
    }
  }

  _matches(sub, event) {
    if (!sub || !event) return false;
    if (sub.path === event.path) {
      const subParams = sub.parameters || {};
      if (Object.keys(subParams).length === 0) return true;
      for (const [k, v] of Object.entries(subParams)) {
        if (JSON.stringify(v) !== JSON.stringify(event.parameters?.[k])) return false;
      }
      return true;
    }
    return false;
  }

  async start() {
    this.vfs.mesh = this;
    for (const url of this.neighborUrls) await this.addPeer(url);
  }

  async addPeer(url) {
    url = url.replace(/\/$/, '');
    log(`[MeshLink ${this.vfs.id}] addPeer startup: Attempting connection to ${url}`);
    if (this.connecting.has(url)) {
        log(`[MeshLink ${this.vfs.id}] addPeer: Already connecting to ${url}, skipping.`);
        return;
    }
    this.connecting.add(url);
    try {
      log(`[MeshLink ${this.vfs.id}] -> POST ${url}/register (vfsId: ${this.vfs.id})`);
      const resp = await this.fetch(`${url}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: this.vfs.id, url: this.localUrl }),
        signal: this.abortController.signal || AbortSignal.timeout(5000),
      });
      log(`[MeshLink ${this.vfs.id}] <- ${resp.status} ${url}/register`);
      if (resp.ok) {
        const info = await resp.json();
        log(`[MeshLink ${this.vfs.id}] Registration successful for ${url}:`, info);
        if (info.id && info.id !== this.vfs.id) {
          let newConn = null;

          if (!this.peers.has(info.id)) {
            newConn = new ForwardConnection(info.id, url, this.fetch, { 
                localUrl: this.localUrl, 
                signal: this.abortController.signal,
                reachability: info.reachability
            });
            this._setPeer(info.id, newConn);
            log(`[MeshLink ${this.vfs.id}] Peer added: ${info.id} (${info.reachability})`);
          }

          // Browser Rule: If we have no local URL, we MUST poll for mesh events
          if (info.reachability === 'REVERSE' || !this.localUrl) {
            const pollerId = `${info.id}-poller`;
            const hasExistingPoller = this.pollers.some(p => p.neighborId === pollerId);
            if (!hasExistingPoller) {
                log(`[MeshLink ${this.vfs.id}] Starting private reverse poll line for ${info.id}`);
                const poller = new ReverseConnection(pollerId, this, { instanceId: this.instanceId });
                this.pollers.push(poller);
                poller.startPolling(url, this.fetch);
            }
          }

          // SYNC INTERESTS TO NEW CONNECTION
          if (newConn) {
              for (const entry of this.interests.values()) {
                let maxExp = entry.localExpiresAt || 0;
                for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
                if (maxExp > Date.now()) {
                  log(`[MeshLink ${this.vfs.id}] -> Syncing interest in ${entry.selector.path} to new peer ${newConn.neighborId}`);
                  newConn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(()=>{});
                }
              }
          }

          this.notify(new Selector('sys/topo'), { peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
          return info.id;
        }
      } else {
        const errText = await resp.text().catch(() => 'no body');
        log(`[MeshLink ${this.vfs.id}] Handshake rejected by ${url} (Status ${resp.status}): ${errText}`);
      }
    } catch (e) {
        const msg = `Handshake failed for ${url}: ${e.message} (Type: ${e.name})`;
        log(`[MeshLink ${this.vfs.id}] ${msg}`);
    } finally { this.connecting.delete(url); }    return null;
  }

  async probeDirectReachability(url) {
    if (!url) return false;
    try {
      const target = `${url.replace(/\/$/, '')}/health`;
      const resp = await this.fetch(target, {
        signal: AbortSignal.timeout(1000),
      });
      const ok = resp.ok;
      log(`[MeshLink ${this.vfs.id}] Probing ${target} -> ${ok ? 'DIRECT' : 'REVERSE'}`);
      return ok;
    } catch (e) {
      log(`[MeshLink ${this.vfs.id}] Probing ${url} -> REVERSE (Error: ${e.message})`);
      return false;
    }
  }

  async registerPeer(peerId, url) {
    if (!peerId) throw new Error('Missing peerId');

    const existing = this.peers.get(peerId);
    if (existing) {
      log(`[MeshLink ${this.vfs.id}] Peer ${peerId} already registered. Reachability: ${existing.reachability}`);
      return { id: this.vfs.id, reachability: existing.reachability };
    }

    if (url) {
      const isReachable = await this.probeDirectReachability(url);
      if (isReachable) {
        const conn = new ForwardConnection(peerId, url, this.fetch, {
          localUrl: this.localUrl,
          signal: this.abortController.signal,
          reachability: 'DIRECT'
        });
        this._setPeer(peerId, conn);
        log(`[MeshLink ${this.vfs.id}] Registered direct peer: ${peerId} at ${url}`);

        // Sync active interests
        for (const entry of this.interests.values()) {
          let maxExp = entry.localExpiresAt || 0;
          for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
          if (maxExp > Date.now()) {
            log(`[MeshLink ${this.vfs.id}] -> Syncing interest in ${entry.selector.path} to new direct peer ${peerId}`);
            conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(() => {});
          }
        }

        this.notify(new Selector('sys/topo'), {
          peer: this.vfs.id,
          neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability }))
        });
        return { id: this.vfs.id, reachability: 'DIRECT' };
      } else {
        log(`[MeshLink ${this.vfs.id}] Direct probe to ${url} failed. Falling back to REVERSE connection.`);
        const conn = new ReverseConnection(peerId, this, { instanceId: this.instanceId });
        this._setPeer(peerId, conn);
        log(`[MeshLink ${this.vfs.id}] Registered reverse peer connection (probe fallback) for: ${peerId}`);

        // Sync active interests
        for (const entry of this.interests.values()) {
          let maxExp = entry.localExpiresAt || 0;
          for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
          if (maxExp > Date.now()) {
            log(`[MeshLink ${this.vfs.id}] -> Syncing interest in ${entry.selector.path} to new reverse peer ${peerId}`);
            conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(() => {});
          }
        }

        this.notify(new Selector('sys/topo'), {
          peer: this.vfs.id,
          neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability }))
        });
        return { id: this.vfs.id, reachability: 'REVERSE' };
      }
    } else {
      const conn = new ReverseConnection(peerId, this, { instanceId: this.instanceId });
      this._setPeer(peerId, conn);
      log(`[MeshLink ${this.vfs.id}] Registered reverse peer connection for: ${peerId}`);

      // Sync active interests
      for (const entry of this.interests.values()) {
        let maxExp = entry.localExpiresAt || 0;
        for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
        if (maxExp > Date.now()) {
          log(`[MeshLink ${this.vfs.id}] -> Syncing interest in ${entry.selector.path} to new reverse peer ${peerId}`);
          conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(() => {});
        }
      }

      this.notify(new Selector('sys/topo'), {
        peer: this.vfs.id,
        neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability }))
      });
      return { id: this.vfs.id, reachability: 'REVERSE' };
    }
  }

  /**
   * SERVER SIDE: Entry point for a hidden peer calling /listen.
   */
  registerReversePeer(neighborId, res, replyTo = null, stream = null, info = null) {
    log(`[MeshLink ${this.vfs.id}] registerReversePeer: ${neighborId} (replyTo: ${replyTo || 'none'})`);
    if (!this.peers.has(neighborId)) {
      const conn = new ReverseConnection(neighborId, this, { instanceId: this.instanceId });
      this._setPeer(neighborId, conn);
      this.notify(new Selector('sys/topo'), { peer: this.vfs.id, neighbors: [...this.peers.values()].map(c => ({ id: c.neighborId, reachability: c.reachability })) });
      for (const entry of this.interests.values()) {
        let maxExp = entry.localExpiresAt || 0;
        for (const exp of entry.subs.values()) if (exp > maxExp) maxExp = exp;
        if (maxExp > Date.now()) {
          conn.subscribe(entry.selector, maxExp, [this.vfs.id]).catch(()=>{});
        }
      }
    }
    const conn = this.peers.get(neighborId);
    if (!(conn instanceof ReverseConnection)) {
        throw new Error(`CRITICAL PROTOCOL VIOLATION: registerReversePeer called for ${neighborId} but connection is ${conn.constructor.name}`);
    }
    conn.addScanner(res, replyTo, stream, info);
  }

  async read(target, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    
    const targetConns = [...this.peers.values()].filter(c => !stack.includes(c.neighborId));
    if (targetConns.length === 0) return null;

    const fetchPromises = targetConns.map(async (conn) => {
      log(`[MeshLink ${this.vfs.id}] Requesting ${target.path || target} from peer: ${conn.neighborId}`);
      const resp = await conn.read(target, { ...context, stack, resolutionStack, expiresAt });
      
      if (resp && resp.body) {
          return {
              stream: resp.body,
              metadata: decodeInfo(resp.headers.get('x-vfs-info'))
          };
      }
      throw new Error(`Connection ${conn.neighborId} failed to return a valid response body.`);
    });

    try { 
        return await Promise.any(fetchPromises); 
    } catch (e) { 
        const msg = `Mesh-wide Read Failure for ${target.path || target}: ${e.message}`;
        console.error(`[MeshLink ${this.vfs.id}] ${msg}`);
        throw new Error(msg);
    } finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  async spy(selector, context = {}) {
    const s = normalizeSelector(selector);
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    
    const nextStack = stack.includes(this.vfs.id) ? stack : [...stack, this.vfs.id];
    const targetConns = [...this.peers.values()].filter(c => !nextStack.includes(c.neighborId));
    
    if (targetConns.length === 0) return null;
    const fetchPromises = targetConns.map(async (conn) => { try { return await conn.spy(s, { ...context, stack: nextStack, expiresAt }); } catch (err) { return null; } });
    try {
      const results = await Promise.allSettled(fetchPromises);
      const validStreams = results.filter(r => r.status === 'fulfilled' && r.value).map(r => r.value);
      if (validStreams.length === 0) return null;
      return new ReadableStream({
        async start(controller) {
          for (const s of validStreams) {
            const reader = s.getReader();
            try { while (true) { const { done, value } = await reader.read(); if (done) break; controller.enqueue(value); } } finally { reader.releaseLock(); }
          }
          controller.close();
        },
      });
    } finally { for (const p of fetchPromises) p.catch(() => {}); }
  }

  stop() { 
    this.abortController.abort(); 
    clearInterval(this._ppsInterval); 
    for (const conn of this.peers.values()) {
        if (conn instanceof ReverseConnection) conn.stop();
    }
    for (const poller of this.pollers) {
        poller.stop();
    }
    this.peers.clear(); 
    this.pollers = [];
  }
}

export class MeshLink extends MeshLinkBase {}
