import { Connection } from './connection.js';
import { Selector, normalizeSelector, decodeInfo } from '../cid.js';
import { log } from '../log.js';

/**
 * ForwardConnection: A pipe using outgoing HTTP requests to a stable neighbor URL.
 * Implements explicit Selector/CID branching to avoid payload guessing.
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

  async readSelector(selector, context = {}) {
    const headers = this._buildHeaders('READ_SELECTOR', context);
    headers['x-vfs-selector'] = JSON.stringify(selector);
    return this._doRequest(`${this.url}/read_selector`, headers, null, selector.path);
  }

  async readCID(cid, context = {}) {
    const headers = this._buildHeaders('READ_CID', context);
    const body = JSON.stringify({ cid, resolutionStack: context.resolutionStack || [] });
    return this._doRequest(`${this.url}/read_cid`, headers, body, cid);
  }

  _buildHeaders(op, context) {
    const { stack = [], expiresAt = Date.now() + 30000 } = context;
    const headers = { 
        'x-vfs-id': stack[0] || 'unknown',
        'x-vfs-op': op,
        'x-vfs-stack': stack.join(','),
        'x-vfs-expires': String(expiresAt)
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

  async spy(selector, context = {}) {
    const headers = this._buildHeaders('SPY', context);
    headers['Content-Type'] = 'application/json';
    headers['x-vfs-selector'] = JSON.stringify(selector);

    const resp = await this.fetch(`${this.url}/spy`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
      body: JSON.stringify({ resolutionStack: context.resolutionStack || [] }),
    });

    if (resp.ok && resp.body) return resp.body;
    return null;
  }

  async subscribe(selector, expiresAt, stack) {
    const s = normalizeSelector(selector);
    const headers = this._buildHeaders('SUB', { stack, expiresAt });
    headers['x-vfs-selector'] = JSON.stringify(s);
    headers['x-vfs-id'] = stack[stack.length - 1];

    log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/subscribe`, s.path);
    await this.fetch(`${this.url}/subscribe`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
    }).catch(() => {});
  }

  async notify(selector, payload, stack = []) {
    const s = normalizeSelector(selector);
    this._pulseCount++;
    this.lastPulse = Date.now();

    const isBinary = payload instanceof Uint8Array;
    const headers = this._buildHeaders('PUB', { stack });
    headers['x-vfs-selector'] = JSON.stringify(s);
    headers['x-vfs-encoding'] = isBinary ? 'bytes' : 'json';
    if (!isBinary) headers['Content-Type'] = 'application/json';

    const body = isBinary ? payload : JSON.stringify(payload);

    log(`[MeshLink ${this.neighborId}] -> POST ${this.url}/notify`, s.path);
    await this.fetch(`${this.url}/notify`, {
      method: 'POST',
      headers,
      signal: this.signal || AbortSignal.timeout(5000),
      body,
    }).catch(() => {});
  }
}
