import { Connection, VFSResult } from './connection.js';
import { log } from '../log.js';

export function encodeRecord(header, payload = null) {
  const headerStr = JSON.stringify(header);
  const headerBytes = new TextEncoder().encode(headerStr);
  
  let payloadBytes;
  if (payload === null || payload === undefined) {
    payloadBytes = new Uint8Array(0);
  } else if (payload instanceof Uint8Array) {
    payloadBytes = payload;
  } else if (typeof payload === 'string') {
    payloadBytes = new TextEncoder().encode(payload);
  } else {
    payloadBytes = new TextEncoder().encode(JSON.stringify(payload));
  }
  
  const recordBytes = new Uint8Array(4 + headerBytes.length + payloadBytes.length);
  const view = new DataView(recordBytes.buffer);
  view.setUint32(0, headerBytes.length, false); // Big Endian
  recordBytes.set(headerBytes, 4);
  recordBytes.set(payloadBytes, 4 + headerBytes.length);
  return recordBytes;
}

export async function decodeRecordStream(readableStream) {
  const reader = readableStream.getReader();
  let buffer = new Uint8Array(0);
  
  function append(chunk) {
    if (!chunk || chunk.length === 0) return;
    const next = new Uint8Array(buffer.length + chunk.length);
    next.set(buffer, 0);
    next.set(chunk, buffer.length);
    buffer = next;
  }
  
  while (buffer.length < 4) {
    const { done, value } = await reader.read();
    if (done) throw new Error("Stream closed before reading length prefix");
    append(value);
  }
  
  const view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
  const headerLen = view.getUint32(0, false);
  
  while (buffer.length < 4 + headerLen) {
    const { done, value } = await reader.read();
    if (done) throw new Error("Stream closed before reading header JSON");
    append(value);
  }
  
  const headerBytes = buffer.subarray(4, 4 + headerLen);
  const header = JSON.parse(new TextDecoder().decode(headerBytes));
  const remaining = buffer.subarray(4 + headerLen);
  
  let released = false;
  const payloadStream = new ReadableStream({
    async start(controller) {
      if (remaining.length > 0) {
        controller.enqueue(remaining);
      }
    },
    async pull(controller) {
      if (released) return;
      const { done, value } = await reader.read();
      if (done) {
        released = true;
        controller.close();
      } else {
        controller.enqueue(value);
      }
    },
    cancel() {
      reader.releaseLock();
    }
  });
  
  return { header, payloadStream };
}

/**
 * ForwardConnection: A pipe using outgoing HTTP requests to a stable neighbor URL.
 * Implements the single unified `send` interface.
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

  getProtocol() {
    return this.url.startsWith('https') ? 'https' : 'http';
  }

  /**
   * Dispatch an encapsulated request to the peer.
   * @param {VFSRequest} req
   * @returns {Promise<VFSResult|void>}
   */
  async send(req) {
    const header = {
      op: req.op,
      stack: req.stack,
      expiresAt: req.expiresAt
    };
    if (this.localUrl) header.localUrl = this.localUrl;

    if (req.op === 'READ_SELECTOR') {
      header.selector = req.selector;
      const body = encodeRecord(header);
      return await this._doRequest(`${this.url}/read_selector`, body, req.selector.path);
    }

    if (req.op === 'READ_CID') {
      header.cid = req.cid;
      header.resolutionStack = req.resolutionStack || [];
      const body = encodeRecord(header);
      return await this._doRequest(`${this.url}/read_cid`, body, req.cid);
    }

    if (req.op === 'SPY') {
      header.selector = req.selector;
      header.resolutionStack = req.resolutionStack || [];
      const body = encodeRecord(header);
      return await this._doRequest(`${this.url}/spy`, body, req.selector.path);
    }

    if (req.op === 'SUB') {
      header.selector = req.selector;
      const body = encodeRecord(header);
      
      log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/subscribe`, req.selector.path);
      const resp = await this.fetch(`${this.url}/subscribe`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream' },
        signal: this.signal || AbortSignal.timeout(5000),
        body,
      });
      log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${this.url}/subscribe`);

      if (!resp.ok) {
        const errText = await resp.text().catch(() => 'no body');
        throw new Error(`HTTP ${resp.status} on SUBSCRIBE: ${errText}`);
      }
    }

    if (req.op === 'PUB') {
      this._pulseCount++;
      this.lastPulse = Date.now();

      const isBinary = req.payload instanceof Uint8Array;
      header.selector = req.selector;
      header.encoding = isBinary ? 'bytes' : 'json';

      const body = encodeRecord(header, req.payload);

      log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/notify`, req.selector.path);
      const resp = await this.fetch(`${this.url}/notify`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/octet-stream' },
        signal: this.signal || AbortSignal.timeout(5000),
        body,
      });
      log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${this.url}/notify`);

      if (!resp.ok) {
        const errText = await resp.text().catch(() => 'no body');
        throw new Error(`HTTP ${resp.status} on NOTIFY: ${errText}`);
      }
    }
  }

  async _doRequest(url, body, targetLabel) {
    try {
        log(`[MeshLink ${this.neighborId}] -> POST ${url}`, targetLabel);
        const resp = await this.fetch(url, {
          method: 'POST',
          headers: { 'Content-Type': 'application/octet-stream' },
          signal: this.signal || AbortSignal.timeout(10000),
          body,
        });
        log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${url}`);

        if (resp.body) {
          try {
            const { header, payloadStream } = await decodeRecordStream(resp.body);
            if (resp.ok) {
              const info = header.info || {};
              if (!info.encoding && header.encoding) {
                info.encoding = header.encoding;
              }
              return new VFSResult(payloadStream, info);
            } else {
              const info = header.info || {};
              const errMsg = info.error || `HTTP ${resp.status}`;
              throw new Error(`Peer ${this.neighborId} reported error: ${errMsg}`);
            }
          } catch (e) {
            if (!resp.ok) {
              const errText = await resp.text().catch(() => 'no body');
              throw new Error(`HTTP ${resp.status}: ${errText}`);
            }
            throw e;
          }
        }
        throw new Error(`Peer ${this.neighborId} returned no body`);
    } catch (err) {
        log(`[MeshLink ${this.neighborId}] request error: ${err.message}`);
        throw err;
    }
  }
}
