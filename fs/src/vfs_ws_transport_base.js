import { Connection } from './mesh_link.js';
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
    this.pendingReads = new Map();
    this.pendingSpies = new Map();
  }

  // Abstract methods to be implemented by platform subclasses
  _send(frame) { throw new Error('Not implemented'); }
  _close() { throw new Error('Not implemented'); }

  async handleFrame(frame) {
    const { txId, type, selector, cid, payload, encoding, stack, expiresAt, status } = frame;
    log(`[WSConnectionBase ${this.neighborId}] INCOMING FRAME: ${type} ${txId || ''}`);

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
          const bytes = fromBase64(payload.data);
          const stream = new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          });

          const headers = new Map();
          headers.set('x-vfs-info', JSON.stringify(payload.metadata));
          headers.set('x-vfs-encoding', payload.metadata.encoding || 'json');
          handler.resolve({ body: stream, headers });
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
            payload: { data: toBase64(buffer) }
          });
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
          const bytes = fromBase64(payload.data);
          const stream = new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          });
          handler.resolve(stream);
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
      const p = encoding === 'bytes' ? fromBase64(payload) : payload;
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
          payload: { data: toBase64(buffer), metadata }
        });
      } else {
        this._send({ txId, type: 'READ_RESPONSE', status: 404 });
      }
  }

  async readSelector(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        if (this.pendingReads.has(txId)) {
            this.pendingReads.delete(txId);
            reject(new Error(`WebSocket VFS read Selector timeout for ${selector.path}`));
        }
      }, Math.max(0, expiresAt - Date.now()));

      this.pendingReads.set(txId, {
        resolve: (val) => { clearTimeout(timeout); resolve(val); },
        reject: (err) => { clearTimeout(timeout); reject(err); }
      });

      this._send({ txId, type: 'READ_SELECTOR', selector, stack, expiresAt });
    });
  }

  async readCID(cid, context = {}) {
    const { stack = [], resolutionStack = [], expiresAt = Date.now() + 30000 } = context;
    const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        if (this.pendingReads.has(txId)) {
            this.pendingReads.delete(txId);
            reject(new Error(`WebSocket VFS read CID timeout for ${cid}`));
        }
      }, Math.max(0, expiresAt - Date.now()));

      this.pendingReads.set(txId, {
        resolve: (val) => { clearTimeout(timeout); resolve(val); },
        reject: (err) => { clearTimeout(timeout); reject(err); }
      });

      this._send({ txId, type: 'READ_CID', cid, resolutionStack, stack, expiresAt });
    });
  }

  async spy(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const txId = globalThis.crypto?.randomUUID ? globalThis.crypto.randomUUID() : Math.random().toString(36).substring(2);

    return new Promise((resolve) => {
      const timeout = setTimeout(() => {
        if (this.pendingSpies.has(txId)) {
            this.pendingSpies.delete(txId);
            resolve(null);
        }
      }, Math.max(0, expiresAt - Date.now()));

      this.pendingSpies.set(txId, {
        resolve: (val) => { clearTimeout(timeout); resolve(val); }
      });

      this._send({ txId, type: 'SPY', selector, stack, expiresAt });
    });
  }

  async subscribe(selector, expiresAt, stack) {
    this._send({ type: 'SUBSCRIBE', selector, expiresAt, stack });
  }

  async notify(selector, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    const isBinary = payload instanceof Uint8Array;
    this._send({
      type: 'NOTIFY',
      selector,
      payload: isBinary ? toBase64(payload) : payload,
      encoding: isBinary ? 'bytes' : 'json',
      stack
    });
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
