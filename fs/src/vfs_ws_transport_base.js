import { Connection, VFSResult } from './mesh_link.js';
import { Selector } from './cid.js';
import { fromBase64, toBase64 } from './encoding.js';
import { log } from './log.js';

/**
 * WSConnectionBase: Environment-agnostic logic for the WebSocket transport.
 * Implements the VFS protocol (READ_SELECTOR, READ_CID, NOTIFY, SUBSCRIBE) 
 * over an abstract socket, honoring Identity Duality.
 */
export class WSConnectionBase extends Connection {
  constructor(neighborId, socket) {
    super(neighborId);
    this.socket = socket;
    this.protocol = 'WS';
    this.pendingReads = new Map();
    this.pendingSpies = new Map();
  }

  // Abstract methods to be implemented by platform subclasses
  _send(frame) { throw new Error('Not implemented'); }
  _close() { throw new Error('Not implemented'); }

  /**
   * Handle an incoming WebSocket frame.
   * Supports sequential binary framing: TEXT frame with hasBinary=true followed by BINARY frame.
   * @param {Object|Uint8Array} data 
   */
  async handleFrame(data) {
    // 1. Handle Binary Frame (the payload for a previously received header)
    if (data instanceof Uint8Array || (typeof Buffer !== 'undefined' && Buffer.isBuffer(data))) {
        if (!this._expectingBinaryFor) {
            log(`[WSConnectionBase ${this.neighborId}] RECEIVED UNEXPECTED BINARY FRAME. Ignoring.`);
            return;
        }
        const frame = this._expectingBinaryFor;
        this._expectingBinaryFor = null;
        
        // Attach raw bytes to the frame
        frame.binaryData = data;
        return this._processCompleteFrame(frame);
    }

    // 2. Handle Text Frame (JSON Envelope)
    let frame;
    try {
        frame = typeof data === 'string' ? JSON.parse(data) : data;
    } catch (err) {
        log(`[WSConnectionBase ${this.neighborId}] FAILED TO PARSE JSON FRAME: ${err.message}`);
        return;
    }

    if (frame.hasBinary) {
        this._expectingBinaryFor = frame;
        return; // Wait for the binary frame
    }

    return this._processCompleteFrame(frame);
  }

  async _processCompleteFrame(frame) {
    const { txId, type, selector, cid, payload, encoding, stack, expiresAt, status, binaryData } = frame;
    log(`[WSConnectionBase ${this.neighborId}] PROCESSING FRAME: ${type} ${txId || ''}`);

    if (type === 'READ_SELECTOR' || (type === 'READ' && selector)) {
      try {
        const target = Selector.fromObject(selector);
        log(`[WSConnectionBase ${this.neighborId}] -> VFS.readSelector(${target.path})`);
        const result = await this.vfs.readSelector(target, { stack, expiresAt });
        this._handleReadResult(txId, result);
      } catch (err) {
        this._send({ txId, type: 'READ_RESPONSE', status: 500, payload: { error: err.message } });
      }
    } 

    else if (type === 'READ_CID' || (type === 'READ' && cid)) {
      try {
        log(`[WSConnectionBase ${this.neighborId}] -> VFS.readCID(${cid})`);
        const rs = frame.resolutionStack || [];
        const result = await this.vfs.readCID(cid, { stack, resolutionStack: rs, expiresAt });
        this._handleReadResult(txId, result);
      } catch (err) {
        this._send({ txId, type: 'READ_RESPONSE', status: 500, payload: { error: err.message } });
      }
    }

    else if (type === 'READ_RESPONSE') {
      const handler = this.pendingReads.get(txId);
      if (handler) {
        this.pendingReads.delete(txId);
        log(`[WSConnectionBase ${this.neighborId}] READ_RESPONSE status: ${status}, txId: ${txId}`);
        if (status === 200) {
          const bytes = binaryData || (payload?.data ? fromBase64(payload.data) : new Uint8Array(0));
          const stream = new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          });

          handler.resolve(new VFSResult(stream, payload?.metadata || frame.metadata));
        } else {
          const errMsg = payload?.error || `VFS Error status ${status}`;
          handler.reject(new Error(errMsg));
        }
      }
    }
    else if (type === 'SPY') {
      try {
        const sel = Selector.fromObject(selector);
        const stream = await this.vfs.spy(sel, { stack, expiresAt });
        if (stream) {
          const chunks = [];
          const reader = stream.getReader();
          while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
          }
          
          let totalLen = chunks.reduce((acc, c) => acc + c.length, 0);
          const buffer = new Uint8Array(totalLen);
          let offset = 0;
          for (const c of chunks) {
              buffer.set(c, offset);
              offset += c.length;
          }

          this._send({
            txId,
            type: 'SPY_RESPONSE',
            status: 200,
            hasBinary: true
          }, buffer);
        } else {
          this._send({ txId, type: 'SPY_RESPONSE', status: 404 });
        }
      } catch (err) {
        this._send({ txId, type: 'SPY_RESPONSE', status: 500, payload: { error: err.message } });
      }
    } 
    
    else if (type === 'SPY_RESPONSE') {
      const handler = this.pendingSpies.get(txId);
      if (handler) {
        this.pendingSpies.delete(txId);
        if (status === 200) {
          const bytes = binaryData || (payload?.data ? fromBase64(payload.data) : new Uint8Array(0));

          const stream = new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          });
          handler.resolve(new VFSResult(stream));
        } else {
          handler.resolve(null);
        }
      }
    } 
    
    else if (type === 'SUBSCRIBE') {
      const sel = Selector.fromObject(selector);
      if (this.mesh) {
        this.mesh.addInterest(this.neighborId, sel, expiresAt, stack);
      } else if (this.vfs) {
        this.vfs.subscribe(sel, expiresAt, stack);
      }
    }
    
    else if (type === 'NOTIFY') {
      const p = binaryData || (encoding === 'bytes' ? fromBase64(payload) : payload);
      this.vfs.notify(Selector.fromObject(selector), p, stack);
    }
  }

  async _handleReadResult(txId, result) {
    if (result) {
        const { stream, metadata } = result;
        const chunks = [];
        const reader = stream.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
        }
        let totalLen = chunks.reduce((acc, c) => acc + c.length, 0);
        const buffer = new Uint8Array(totalLen);
        let offset = 0;
        for (const c of chunks) {
            buffer.set(c, offset);
            offset += c.length;
        }
        this._send({
          txId,
          type: 'READ_RESPONSE',
          status: 200,
          metadata,
          hasBinary: true
        }, buffer);
      } else {
        this._send({ txId, type: 'READ_RESPONSE', status: 404 });
      }
  }

  /**
   * Dispatch an encapsulated request to the peer.
   * @param {VFSRequest} req
   * @returns {Promise<VFSResult|void>}
   */
  async send(req) {
    if (req.op === 'READ_SELECTOR') {
      const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          if (this.pendingReads.has(txId)) {
              this.pendingReads.delete(txId);
              reject(new Error(`WebSocket VFS read Selector timeout for ${req.selector.path}`));
          }
        }, Math.max(0, req.expiresAt - Date.now()));

        this.pendingReads.set(txId, {
          resolve: (val) => { clearTimeout(timeout); resolve(val); },
          reject: (err) => { clearTimeout(timeout); reject(err); }
        });

        this._send({ txId, type: 'READ_SELECTOR', selector: req.selector, stack: req.stack, expiresAt: req.expiresAt });
      });
    }

    if (req.op === 'READ_CID') {
      const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          if (this.pendingReads.has(txId)) {
              this.pendingReads.delete(txId);
              reject(new Error(`WebSocket VFS read CID timeout for ${req.cid}`));
          }
        }, Math.max(0, req.expiresAt - Date.now()));

        this.pendingReads.set(txId, {
          resolve: (val) => { clearTimeout(timeout); resolve(val); },
          reject: (err) => { clearTimeout(timeout); reject(err); }
        });

        this._send({ txId, type: 'READ_CID', cid: req.cid, resolutionStack: req.resolutionStack, stack: req.stack, expiresAt: req.expiresAt });
      });
    }

    if (req.op === 'SPY') {
      const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          if (this.pendingSpies.has(txId)) {
              this.pendingSpies.delete(txId);
              reject(new Error(`WebSocket spy timeout for ${req.selector.path}`));
          }
        }, Math.max(0, req.expiresAt - Date.now()));

        this.pendingSpies.set(txId, {
          resolve: (val) => { clearTimeout(timeout); resolve(val); },
          reject: (err) => { clearTimeout(timeout); reject(err); }
        });

        this._send({ txId, type: 'SPY', selector: req.selector, stack: req.stack, expiresAt: req.expiresAt });
      });
    }

    if (req.op === 'SUB') {
      this._send({ type: 'SUBSCRIBE', selector: req.selector, expiresAt: req.expiresAt, stack: req.stack });
    }

    if (req.op === 'PUB') {
      this._pulseCount++;
      this.lastPulse = Date.now();
      const isBinary = req.payload instanceof Uint8Array;
      if (isBinary) {
          this._send({
            type: 'NOTIFY',
            selector: req.selector,
            encoding: 'bytes',
            stack: req.stack,
            hasBinary: true
          }, req.payload);
      } else {
          this._send({
            type: 'NOTIFY',
            selector: req.selector,
            payload: req.payload,
            encoding: 'json',
            stack: req.stack
          });
      }
    }
  }

  stop() {
    this._close();
    for (const handler of this.pendingReads.values()) {
      handler.reject(new Error('WebSocket connection closed'));
    }
    this.pendingReads.clear();
    this.pendingSpies.clear();
  }
}
