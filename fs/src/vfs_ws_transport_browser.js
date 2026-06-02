import { WSConnectionBase } from './vfs_ws_transport_base.js';
import { log } from './log.js';

/**
 * WSBrowserConnection: Browser implementation of the WebSocket transport.
 * Uses native globalThis.WebSocket and Web streams.
 */
export class WSBrowserConnection extends WSConnectionBase {
  constructor(neighborId, ws) {
    super(neighborId, ws);
    
    this.ws = ws;
    this.ws.binaryType = 'arraybuffer';
    this.ws.onmessage = async (event) => {
      try {
        if (event.data instanceof ArrayBuffer) {
            await this.handleFrame(new Uint8Array(event.data));
        } else {
            await this.handleFrame(event.data);
        }
      } catch (err) {
        log(`[WSBrowserConnection ${this.neighborId}] Frame processing error: ${err.message}`);
      }
    };

    this.ws.onerror = (err) => {
      log(`[WSBrowserConnection ${this.neighborId}] Socket error: ${err.message}`);
    };

    this.ws.onclose = () => {
      log(`[WSBrowserConnection ${this.neighborId}] Socket closed.`);
    };
  }

  _send(frame, payload) {
    if (this.ws.readyState === 1) { // OPEN
      this.ws.send(JSON.stringify(frame));
      if (payload) {
          this.ws.send(payload);
      }
    }
  }

  _close() {
    this.ws.close();
  }
}

export class WSBrowserForwardConnection extends WSBrowserConnection {
  static async connect(url, localId, peerId) {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);
      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error(`WebSocket handshake timeout for ${url}`));
      }, 5000);

      ws.onopen = () => {
        ws.send(JSON.stringify({ type: 'IDENTIFY', peerId: localId }));
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'ACK') {
            clearTimeout(timeout);
            resolve(new WSBrowserForwardConnection(peerId, ws));
          }
        } catch (err) {
          clearTimeout(timeout);
          ws.close();
          reject(new Error(`Handshake error: ${err.message}`));
        }
      };

      ws.onerror = (err) => {
        clearTimeout(timeout);
        reject(err);
      };
    });
  }

  constructor(neighborId, ws) {
    super(neighborId, ws);
    this.reachability = 'DIRECT';
  }
}
