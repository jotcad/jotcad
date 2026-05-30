import { WSConnectionBase } from './vfs_ws_transport_base.js';
import { log } from './log.js';

/**
 * WSNodeConnection: Node.js implementation of the WebSocket transport.
 * Uses the 'ws' library.
 */
export class WSNodeConnection extends WSConnectionBase {
  constructor(neighborId, ws) {
    super(neighborId, ws);
    
    this.ws = ws;
    this.ws.on('message', async (data, isBinary) => {
      try {
        if (isBinary) {
            await this.handleFrame(new Uint8Array(data));
        } else {
            await this.handleFrame(data.toString());
        }
      } catch (err) {
        log(`[WSNodeConnection ${this.neighborId}] Frame processing error: ${err.message}`);
      }
    });

    this.ws.on('error', (err) => {
      log(`[WSNodeConnection ${this.neighborId}] Socket error: ${err.message}`);
    });

    this.ws.on('close', () => {
      log(`[WSNodeConnection ${this.neighborId}] Socket closed.`);
    });
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

export class WSNodeForwardConnection extends WSNodeConnection {
  static async connect(url, localId, peerId) {
    const WebSocket = (await import('ws')).default;
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);
      const timeout = setTimeout(() => {
        ws.close();
        reject(new Error(`WebSocket handshake timeout for ${url}`));
      }, 5000);

      ws.on('open', () => {
        ws.send(JSON.stringify({ type: 'IDENTIFY', peerId: localId }));
      });

      const onHandshake = (data) => {
        try {
          const msg = JSON.parse(data.toString());
          if (msg.type === 'ACK') {
            clearTimeout(timeout);
            ws.off('message', onHandshake);
            resolve(new WSNodeForwardConnection(peerId, ws));
          }
        } catch (err) {
          clearTimeout(timeout);
          ws.off('message', onHandshake);
          ws.close();
          reject(new Error(`Handshake error: ${err.message}`));
        }
      };

      ws.on('message', onHandshake);

      ws.on('error', (err) => {
        clearTimeout(timeout);
        reject(err);
      });
    });
  }

  constructor(neighborId, ws) {
    super(neighborId, ws);
    this.reachability = 'DIRECT';
  }
}

export class WSNodeReverseConnection extends WSNodeConnection {
  constructor(neighborId, ws) {
    super(neighborId, ws);
    this.reachability = 'REVERSE';
  }
}
