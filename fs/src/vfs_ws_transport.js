import { Connection } from './mesh_link.js';
import { Selector, fromBase64 } from './cid.js';
import { log } from './log.js';

export class WSConnectionBase extends Connection {
  constructor(neighborId, ws) {
    super(neighborId);
    this.ws = ws;
    this.pendingReads = new Map();
    this.pendingSpies = new Map();

    this.ws.on('message', async (data) => {
      try {
        const frame = JSON.parse(data.toString());
        await this.handleFrame(frame);
      } catch (err) {
        log(`[WSConnection ${this.neighborId}] Frame processing error: ${err.message}`);
      }
    });

    this.ws.on('error', (err) => {
      log(`[WSConnection ${this.neighborId}] Socket error: ${err.message}`);
    });
  }

  async handleFrame(frame) {
    const { txId, type, selector, cid, payload, encoding, stack, expiresAt, status } = frame;

    if (type === 'READ') {
      try {
        const target = selector ? Selector.fromObject(selector) : cid;
        const result = await this.vfs.read(target, { stack, expiresAt });
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

          log(`[WSConnection READ] Payload buffer length: ${buffer.length}`);
          
          const toBase64 = (bytes) => {
              if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
              let binary = '';
              for (let i = 0; i < bytes.byteLength; i++) binary += String.fromCharCode(bytes[i]);
              return btoa(binary);
          };

          this.ws.send(JSON.stringify({
            txId,
            type: 'READ_RESPONSE',
            status: 200,
            payload: {
              data: toBase64(buffer),
              metadata
            }
          }));
        } else {
          this.ws.send(JSON.stringify({ txId, type: 'READ_RESPONSE', status: 404 }));
        }
      } catch (err) {
        this.ws.send(JSON.stringify({ txId, type: 'READ_RESPONSE', status: 500, payload: { error: err.message } }));
      }
    } 
    
    else if (type === 'READ_RESPONSE') {
      const handler = this.pendingReads.get(txId);
      if (handler) {
        this.pendingReads.delete(txId);
        if (status === 200) {
          const bytes = fromBase64(payload.data);
          log(`[WSConnection READ_RESPONSE] Received buffer length: ${bytes.length}`);
          
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

          const toBase64 = (bytes) => {
              if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
              let binary = '';
              for (let i = 0; i < bytes.byteLength; i++) binary += String.fromCharCode(bytes[i]);
              return btoa(binary);
          };

          this.ws.send(JSON.stringify({
            txId,
            type: 'SPY_RESPONSE',
            status: 200,
            payload: { data: toBase64(buffer) }
          }));
        } else {
          this.ws.send(JSON.stringify({ txId, type: 'SPY_RESPONSE', status: 404 }));
        }
      } catch (err) {
        this.ws.send(JSON.stringify({ txId, type: 'SPY_RESPONSE', status: 500, payload: { error: err.message } }));
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
      this.mesh.addInterest(this.neighborId, sel, expiresAt, stack);
    } 
    
    else if (type === 'NOTIFY') {
      const sel = Selector.fromObject(selector);
      let actualPayload = payload;
      if (encoding === 'bytes') {
        actualPayload = fromBase64(payload);
      }
      this.mesh.notify(sel, actualPayload, stack);
    }
  }

  async read(target, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const txId = globalThis.crypto.randomUUID();
    const isSelector = target instanceof Selector;

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pendingReads.delete(txId);
        reject(new Error(`WebSocket VFS read timeout for ${isSelector ? target.path : target}`));
      }, Math.max(0, expiresAt - Date.now()));

      this.pendingReads.set(txId, {
        resolve: (val) => { clearTimeout(timeout); resolve(val); },
        reject: (err) => { clearTimeout(timeout); reject(err); }
      });

      this.ws.send(JSON.stringify({
        txId,
        type: 'READ',
        selector: isSelector ? target : null,
        cid: isSelector ? null : target,
        stack,
        expiresAt
      }));
    });
  }

  async spy(selector, context = {}) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const txId = globalThis.crypto.randomUUID();

    return new Promise((resolve) => {
      const timeout = setTimeout(() => {
        this.pendingSpies.delete(txId);
        resolve(null);
      }, Math.max(0, expiresAt - Date.now()));

      this.pendingSpies.set(txId, {
        resolve: (val) => { clearTimeout(timeout); resolve(val); }
      });

      this.ws.send(JSON.stringify({
        txId,
        type: 'SPY',
        selector,
        stack,
        expiresAt
      }));
    });
  }

  async subscribe(selector, expiresAt, stack) {
    this.ws.send(JSON.stringify({
      type: 'SUBSCRIBE',
      selector,
      expiresAt,
      stack
    }));
  }

  async notify(selector, payload, stack = []) {
    this._pulseCount++;
    this.lastPulse = Date.now();
    const isBinary = payload instanceof Uint8Array;
    
    const toBase64 = (bytes) => {
        if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
        let binary = '';
        for (let i = 0; i < bytes.byteLength; i++) binary += String.fromCharCode(bytes[i]);
        return btoa(binary);
    };

    this.ws.send(JSON.stringify({
      type: 'NOTIFY',
      selector,
      payload: isBinary ? toBase64(payload) : payload,
      encoding: isBinary ? 'bytes' : 'json',
      stack
    }));
  }
}

export class WSForwardConnection extends WSConnectionBase {
  constructor(neighborId, ws) {
    super(neighborId, ws);
    this.reachability = 'DIRECT';
  }
}

export class WSReverseConnection extends WSConnectionBase {
  constructor(neighborId, ws) {
    super(neighborId, ws);
    this.reachability = 'REVERSE';
  }
}
