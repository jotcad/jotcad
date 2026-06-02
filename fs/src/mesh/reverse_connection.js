import { Connection, VFSResult } from './connection.js';
import { Selector, decodeInfo, decodeSafe, normalizeSelector } from '../cid.js';
import { log, info } from '../log.js';

const PEER_POLL_TIMEOUT = 10000;

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
    this.lastPollAt = Date.now();
    
    // Client-side state
    this.abortController = new AbortController();
  }

  getProtocol() {
    if (this.baseUrl) {
      return this.baseUrl.startsWith('https') ? 'https' : 'http';
    }
    return 'http';
  }

  stop() {
    this.abortController.abort();
    for (const { res } of this.pool) {
      try { res.end(); } catch (e) {}
    }
    this.pool = [];
  }

  /**
   * SERVER SIDE: Entry point for a hidden peer calling /listen.
   */
  addScanner(res, replyTo = null, stream = null, info = null) {
    this.lastPollAt = Date.now();
    // Protocol Rule: Permanent Tunnel
    if (res.setTimeout) res.setTimeout(0);

    // Protocol Rule: Superseding
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
      this._flushQueue(res);
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
    if (Date.now() - this.lastPollAt > PEER_POLL_TIMEOUT) {
      log(`[ReverseConn ${this.neighborId}] Pruning stale peer (no poll for ${Date.now() - this.lastPollAt}ms).`);
      this.queue = [];
      // Note: We don't delete from mesh.peers here as the mesh might have multiple connections
      // but we return false to indicate dispatch failed.
      return false;
    }

    info(`[ReverseConn ${this.neighborId}] Queueing command ${command.op || command.type}. Current depth: ${this.queue.length + 1}`);
    this.queue.push(command);
    
    if (this.pool.length > 0) {
      const { res } = this.pool.shift();
      this._flushQueue(res);
      return true;
    }
    return false;
  }

  _flushQueue(res) {
    if (this.queue.length === 0) {
        res.writeHead(204);
        res.end();
        return;
    }

    // BINARY RULE: If the first item is binary, send it as a raw response immediately.
    // This prevents binary payloads (like camera frames) from stalling the queue.
    if (this.queue[0].payload instanceof Uint8Array) {
        const command = this.queue.shift();
        const headers = { 
            'Content-Type': 'application/octet-stream',
            'x-vfs-op': command.op || command.type,
            'x-vfs-encoding': 'bytes'
        };
        if (command.selector) headers['x-vfs-selector'] = JSON.stringify(command.selector);
        if (command.stack) headers['x-vfs-stack'] = command.stack.join(',');
        if (command.id) headers['x-vfs-reply-to'] = command.id;
        res.writeHead(200, headers);
        res.end(command.payload);
        return;
    }

    // BATCH RULE: Collect all contiguous non-binary messages
    const batch = [];
    while (this.queue.length > 0 && !(this.queue[0].payload instanceof Uint8Array)) {
        batch.push(this.queue.shift());
    }

    log(`[ReverseConn ${this.neighborId}] Delivering BATCH of ${batch.length} commands.`);
    res.writeHead(200, { 
        'Content-Type': 'application/json',
        'x-vfs-op': 'BATCH',
        'x-vfs-encoding': 'json'
    });
    res.end(JSON.stringify(batch));
  }

  /**
   * Dispatch an encapsulated request to the peer.
   * @param {VFSRequest} req
   * @returns {Promise<VFSResult|void>}
   */
  async send(req) {
    if (req.op === 'READ_SELECTOR') {
      const requestId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).slice(2);
      const replyPromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          this.replies.delete(requestId);
          reject(new Error(`Reverse read Selector timeout for ${req.selector.path}`));
        }, Math.max(0, req.expiresAt - Date.now()));

        this.replies.set(requestId, (stream, headers = new Map(), err = null) => {
          clearTimeout(timeout);
          if (err) reject(err);
          else {
            const info = decodeInfo(headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
            resolve(new VFSResult(stream, info));
          }
        });
      });

      this._dispatch({ 
          type: 'COMMAND', 
          op: 'READ_SELECTOR', 
          id: requestId, 
          selector: req.selector, 
          stack: req.stack, 
          expiresAt: req.expiresAt 
      });
      return replyPromise;
    }

    if (req.op === 'READ_CID') {
      const requestId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).slice(2);
      const replyPromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          this.replies.delete(requestId);
          reject(new Error(`Reverse read CID timeout for ${req.cid}`));
        }, Math.max(0, req.expiresAt - Date.now()));

        this.replies.set(requestId, (stream, headers = new Map(), err = null) => {
          clearTimeout(timeout);
          if (err) reject(err);
          else {
            const info = decodeInfo(headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
            resolve(new VFSResult(stream, info));
          }
        });
      });

      this._dispatch({ 
          type: 'COMMAND', 
          op: 'READ_CID', 
          id: requestId, 
          payload: { cid: req.cid, resolutionStack: req.resolutionStack || [] }, 
          stack: req.stack, 
          expiresAt: req.expiresAt 
      });
      return replyPromise;
    }

    if (req.op === 'SPY') {
      const requestId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).slice(2);
      const replyPromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          this.replies.delete(requestId);
          reject(new Error(`Reverse spy timeout for ${req.selector.path}`));
        }, Math.max(0, req.expiresAt - Date.now()));

        this.replies.set(requestId, (stream, headers = new Map(), err = null) => {
          clearTimeout(timeout);
          if (err) reject(err);
          else {
            const info = decodeInfo(headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
            resolve(new VFSResult(stream, info));
          }
        });
      });

      this._dispatch({ 
          type: 'COMMAND', 
          op: 'SPY', 
          id: requestId, 
          selector: req.selector, 
          stack: req.stack, 
          resolutionStack: req.resolutionStack || [],
          expiresAt: req.expiresAt 
      });
      return replyPromise;
    }

    if (req.op === 'SUB') {
      this._dispatch({ type: 'COMMAND', op: 'SUB', selector: req.selector, expiresAt: req.expiresAt, stack: req.stack });
    }

    if (req.op === 'PUB') {
      this._pulseCount++;
      this.lastPulse = Date.now();
      this._dispatch({ type: 'PUB', selector: req.selector, payload: req.payload, stack: req.stack });
    }
  }

  async startPolling(baseUrl, fetch) {
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.fetch = fetch;
    log(`[ReverseConn ${this.neighborId}] Instance ${this.instanceId} starting Poll-based Receiver for ${baseUrl}`);
    let replyTo = null, stream = null, metadata = null;
    let consecutiveFailures = 0;
    const MAX_FAILURES = 5;
    let lastPollFinished = Date.now();
    
    while (!this.abortController.signal.aborted) {
      try {
        const pollStart = Date.now();
        const headers = { 'x-vfs-peer-id': this.vfs.id };
        if (replyTo) headers['x-vfs-reply-to'] = replyTo;
         if (metadata) {
            const str = JSON.stringify(metadata);
            headers['x-vfs-info'] = typeof Buffer !== 'undefined'
              ? Buffer.from(str).toString('base64')
              : btoa(unescape(encodeURIComponent(str)));
         }
        
        let bodyToSend = stream;
        const fetchOptions = { method: 'POST', headers, signal: this.abortController.signal };

        if (stream instanceof ReadableStream || (stream && typeof stream.getReader === 'function')) {
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
        
        const resp = await fetch(`${baseUrl}/listen`, fetchOptions);
        const pollEnd = Date.now();
        const pollDuration = pollEnd - pollStart;
        const pollGap = pollStart - lastPollFinished;
        
        replyTo = null; stream = null; metadata = null;
        consecutiveFailures = 0;
        lastPollFinished = pollEnd;

        if (resp.status === 200) {
          const h = resp.headers;
          const op = h?.get('x-vfs-op');
          info(`[ReverseConn ${this.neighborId}] RECEIVED ${op || 'EVENT'} (Poll: ${pollDuration}ms, Gap: ${pollGap}ms)`);
          const encoding = h?.get('x-vfs-encoding');
          const selectorB64 = h?.get('x-vfs-selector');
          const stackStr = h?.get('x-vfs-stack');
          const expiresStr = h?.get('x-vfs-expires');
          const replyToHeader = h?.get('x-vfs-reply-to');

          const processCommand = async (rawCmd) => {
            const cmd = { ...rawCmd };
            if (rawCmd.type === 'COMMAND') {
              const sel = cmd.selector ? Selector.fromObject(cmd.selector) : null;
              if (cmd.op === 'READ_SELECTOR' || (cmd.op === 'READ' && sel)) {
                try {
                  const result = await this.vfs.readSelector(sel, { stack: cmd.stack, expiresAt: cmd.expiresAt });
                  if (result) { stream = result.stream; metadata = result.metadata; } 
                  else { metadata = { error: 'Not Found' }; }
                } catch (readErr) {
                  metadata = { error: readErr.message };
                }
                replyTo = cmd.id;
              } else if (cmd.op === 'READ_CID' || cmd.op === 'READ') {
                try {
                  const cid = cmd.cid || cmd.payload?.cid;
                  const rs = cmd.resolutionStack || cmd.payload?.resolutionStack || [];
                  const result = await this.vfs.readCID(cid, { stack: cmd.stack, resolutionStack: rs, expiresAt: cmd.expiresAt });
                  if (result) { stream = result.stream; metadata = result.metadata; } 
                  else { metadata = { error: 'Not Found' }; }
                } catch (readErr) {
                  metadata = { error: readErr.message };
                }
                replyTo = cmd.id;
              } else if (cmd.op === 'SPY') {
                try {
                  stream = await this.vfs.spy(sel, { stack: cmd.stack, resolutionStack: cmd.resolutionStack, expiresAt: cmd.expiresAt });
                  if (!stream) metadata = { error: 'Not Found' };
                } catch (spyErr) {
                  metadata = { error: spyErr.message };
                }
                replyTo = cmd.id;
              } else if (cmd.op === 'SUB') {
                this.mesh.addInterest(cmd.stack[0] || 'unknown', sel, cmd.expiresAt, cmd.stack);
              }
            } else if (cmd.type === 'PUB') {
              const s = cmd.selector ? Selector.fromObject(cmd.selector) : null;
              this.mesh.notify(s, cmd.payload, cmd.stack);
            }
          };

          if (op === 'BATCH') {
            const batch = JSON.parse(await resp.text());
            info(`[ReverseConn ${this.neighborId}] Processing BATCH of ${batch.length} commands.`);
            for (const item of batch) {
                await processCommand(item);
            }
            continue;
          }

          let cmd, payload;
          if (op) {
            let selector = null;
            if (selectorB64) {
                try { selector = Selector.fromObject(JSON.parse(selectorB64)); } 
                catch (e) { try { selector = decodeSafe(selectorB64); } catch (e2) {} }
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
            cmd = await resp.json();
          }

          info(`[ReverseConn ${this.neighborId}] RECEIVED ${cmd.op || cmd.type}`);
          await processCommand(cmd);

        } else if (resp.status === 409) {
            throw new Error(`CRITICAL: Remote node rejected our poll: ${await resp.text()}`);
        }
      } catch (err) { 
        if (this.abortController.signal.aborted || err.name === 'AbortError') break; 
        consecutiveFailures++;
        if (consecutiveFailures >= MAX_FAILURES) throw err;
        await new Promise((resolve) => setTimeout(resolve, 2000));
      }
    }
  }
}
