import { Connection, VFSResult } from './connection.js';
import { decodeInfo } from '../cid.js';
import { log } from '../log.js';

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
    if (req.op === 'READ_SELECTOR') {
      const headers = this._buildHeaders('READ_SELECTOR', req);
      headers['x-vfs-selector'] = JSON.stringify(req.selector);
      const resp = await this._doRequest(`${this.url}/read_selector`, headers, null, req.selector.path);
      const info = decodeInfo(resp.headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
      return new VFSResult(resp.body, info);
    }

    if (req.op === 'READ_CID') {
      const headers = this._buildHeaders('READ_CID', req);
      const body = JSON.stringify({ cid: req.cid, resolutionStack: req.resolutionStack || [] });
      const resp = await this._doRequest(`${this.url}/read_cid`, headers, body, req.cid);
      const info = decodeInfo(resp.headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
      return new VFSResult(resp.body, info);
    }

    if (req.op === 'SPY') {
      const headers = this._buildHeaders('SPY', req);
      headers['Content-Type'] = 'application/json';
      headers['x-vfs-selector'] = JSON.stringify(req.selector);
      const body = JSON.stringify({ resolutionStack: req.resolutionStack || [] });
      
      log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/spy`, req.selector.path);
      const resp = await this.fetch(`${this.url}/spy`, {
        method: 'POST',
        headers,
        signal: this.signal || AbortSignal.timeout(5000),
        body,
      });
      log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${this.url}/spy`);

      if (!resp.ok) {
        const errText = await resp.text().catch(() => 'no body');
        throw new Error(`HTTP ${resp.status} on SPY: ${errText}`);
      }
      const info = decodeInfo(resp.headers.get('x-vfs-info')) || { state: 'AVAILABLE' };
      return new VFSResult(resp.body, info);
    }

    if (req.op === 'SUB') {
      const headers = this._buildHeaders('SUB', req);
      headers['x-vfs-selector'] = JSON.stringify(req.selector);
      if (req.stack.length > 0) {
        headers['x-vfs-id'] = req.stack[req.stack.length - 1];
      }

      log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/subscribe`, req.selector.path);
      const resp = await this.fetch(`${this.url}/subscribe`, {
        method: 'POST',
        headers,
        signal: this.signal || AbortSignal.timeout(5000),
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
      const headers = this._buildHeaders('PUB', req);
      headers['x-vfs-selector'] = JSON.stringify(req.selector);
      headers['x-vfs-encoding'] = isBinary ? 'bytes' : 'json';
      if (!isBinary) headers['Content-Type'] = 'application/json';

      const body = isBinary ? req.payload : JSON.stringify(req.payload);

      log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/notify`, req.selector.path);
      const resp = await this.fetch(`${this.url}/notify`, {
        method: 'POST',
        headers,
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

  _buildHeaders(op, req) {
    const headers = { 
        'x-vfs-id': req.stack[0] || 'unknown',
        'x-vfs-op': op,
        'x-vfs-stack': req.stack.join(','),
        'x-vfs-expires': String(req.expiresAt)
    };
    if (this.localUrl) headers['x-vfs-local-url'] = this.localUrl;
    return headers;
  }

  async _doRequest(url, headers, body, targetLabel) {
    try {
        log(`[MeshLink ${this.neighborId}] -> POST ${url} (${headers['x-vfs-op']})`, targetLabel);
        const resp = await this.fetch(url, {
          method: 'POST',
          headers,
          signal: this.signal || AbortSignal.timeout(10000),
          body,
        });
        log(`[MeshLink ${this.neighborId}] <- ${resp.status} ${url}`);

        if (resp.ok && resp.body) return { body: resp.body, headers: resp.headers };
        
        const info = decodeInfo(resp.headers.get('x-vfs-info'));
        if (info.error) {
            throw new Error(`Peer ${this.neighborId} reported error: ${info.error}`);
        }

        const errText = await resp.text().catch(() => 'no body');
        throw new Error(`HTTP ${resp.status}: ${errText}`);
    } catch (err) {
        log(`[MeshLink ${this.neighborId}] request error: ${err.message}`);
        throw err;
    }
  }
}
