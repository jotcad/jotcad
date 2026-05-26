/**
 * Selector: The universal address for mesh content.
 * MUST be defined at the top to avoid hoisting issues with encodeSelectorJCB.
 */
export class Selector {
  constructor(path, parameters = {}) {
    if (arguments.length > 2) {
      throw new Error('Selector constructor only accepts (path, parameters). Use .withOutput(out) for output ports.');
    }
    this.path = path;
    this.parameters = parameters;
    this._isJotSelector = true; // Branding for context-safe identification
  }

  withOutput(output) {
    const s = new Selector(this.path, this.parameters);
    s.output = output;
    return s;
  }

  toJSON() {
    const res = { path: this.path, parameters: this.parameters };
    if (this.output) res.output = this.output;
    return res;
  }

  static fromObject(obj) {
    if (!obj) return null;
    const s = new Selector(obj.path, obj.parameters);
    if (obj.output) s.output = obj.output;
    return s;
  }
}

/**
 * isSelector: Context-safe type guard.
 */
export function isSelector(s) {
    return s && (s instanceof Selector || s._isJotSelector === true);
}

/**
 * normalizeSelector: Enforces strict Selector type.
 */
export function normalizeSelector(s) {
  if (isSelector(s)) return s;

  const errorMsg = `CRITICAL PROTOCOL VIOLATION: Received raw object where a formal Selector instance was required. ` +
                   `This indicates a boundary hydration leak (Selector.fromObject() was missed). ` +
                   `Input type: ${s?.constructor?.name || typeof s}`;
  
  console.error(errorMsg, s);
  throw new Error(errorMsg);
}

/**
 * toBase64: Standard Base64 encoding for Uint8Array.
 */
function toBase64(bytes) {
  if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
  let binary = '';
  const len = (bytes.byteLength !== undefined) ? bytes.byteLength : bytes.length;
  for (let i = 0; i < len; i++) binary += String.fromCharCode(bytes[i]);
  return btoa(binary);
}

/**
 * fromBase64: Standard Base64 decoding to Uint8Array.
 */
function fromBase64(base64) {
  if (typeof Buffer !== 'undefined') return new Uint8Array(Buffer.from(base64, 'base64'));
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/**
 * encodeDataJCB: Deterministic Binary Encoding for any structured data.
 * Used for hashing content (CIDs).
 */
export function encodeDataJCB(val) {
  const chunks = [];
  function walk(v) {
    if (v === null || v === undefined) {
      chunks.push(new Uint8Array([0x01]));
    } else if (typeof v === 'boolean') {
      chunks.push(new Uint8Array([0x02, v ? 1 : 0]));
    } else if (typeof v === 'number') {
      const b = new Uint8Array(9);
      b[0] = 0x03;
      const view = new DataView(b.buffer);
      view.setFloat64(1, v, false); // Big Endian
      chunks.push(b);
    } else if (typeof v === 'string') {
      const bytes = new TextEncoder().encode(v);
      const header = new Uint8Array(5);
      header[0] = 0x04;
      const view = new DataView(header.buffer);
      view.setUint32(1, bytes.length, false);
      chunks.push(header, bytes);
    } else if (Array.isArray(v)) {
      const header = new Uint8Array(5);
      header[0] = 0x05;
      const view = new DataView(header.buffer);
      view.setUint32(1, v.length, false);
      chunks.push(header);
      for (const item of v) walk(item);
    } else if (typeof v === 'object') {
      // Use toJSON if available (for Selector instances)
      const data = (v && typeof v.toJSON === 'function') ? v.toJSON() : v;
      const keys = Object.keys(data).sort();
      const header = new Uint8Array(5);
      header[0] = 0x06;
      const view = new DataView(header.buffer);
      view.setUint32(1, keys.length, false);
      chunks.push(header);
      for (const k of keys) {
        walk(k);
        walk(data[k]);
      }
    }
  }
  walk(val);
  let total = chunks.reduce((acc, c) => acc + c.length, 0);
  const result = new Uint8Array(total);
  let offset = 0;
  for (const c of chunks) { result.set(c, offset); offset += c.length; }
  return result;
}

/**
 * encodeSelectorJCB: Deterministic Binary Encoding for ADDRESSES.
 * Enforces strict Selector type.
 */
export function encodeSelectorJCB(val) {
  if (!isSelector(val)) {
    console.error('[CID DEBUG] encodeSelectorJCB failed check.', {
        val,
        isSelector: isSelector(val),
        constructor: val?.constructor?.name,
        proto: Object.getPrototypeOf(val)?.constructor?.name,
        isJot: val?._isJotSelector
    });
    throw new Error(`CRITICAL: encodeSelectorJCB requires a formal Selector instance. Got: ${val?.constructor?.name || typeof val}.`);
  }
  return encodeDataJCB(val);
}

/**
 * encodeJCB: General purpose alias for data encoding.
 */
export const encodeJCB = encodeDataJCB;

/**
 * decodeDataJCB: Decodes JCB bytes back into JS values.
 */
export function decodeDataJCB(bytes) {
  let offset = 0;
  function walk() {
    if (offset >= bytes.length) return undefined;
    const tag = bytes[offset++];
    if (tag === 0x01) return null;
    if (tag === 0x02) return bytes[offset++] === 1;
    if (tag === 0x03) {
      const view = new DataView(bytes.buffer, bytes.byteOffset + offset, 8);
      offset += 8;
      return view.getFloat64(0, false);
    }
    if (tag === 0x04) {
      const view = new DataView(bytes.buffer, bytes.byteOffset + offset, 4);
      const len = view.getUint32(0, false);
      offset += 4;
      const s = new TextDecoder().decode(bytes.subarray(offset, offset + len));
      offset += len;
      return s;
    }
    if (tag === 0x05) {
      const view = new DataView(bytes.buffer, bytes.byteOffset + offset, 4);
      const len = view.getUint32(0, false);
      offset += 4;
      const arr = [];
      for (let i = 0; i < len; i++) arr.push(walk());
      return arr;
    }
    if (tag === 0x06) {
      const view = new DataView(bytes.buffer, bytes.byteOffset + offset, 4);
      const len = view.getUint32(0, false);
      offset += 4;
      const obj = {};
      for (let i = 0; i < len; i++) {
        const k = walk();
        const v = walk();
        obj[k] = v;
      }
      return obj;
    }
    throw new Error(`Invalid JCB tag at offset ${offset-1}: ${tag}`);
  }
  return walk();
}

/**
 * decodeSelectorJCB: Decodes JCB bytes into a formal Selector instance.
 */
export function decodeSelectorJCB(bytes) {
    const obj = decodeDataJCB(bytes);
    if (!obj || typeof obj !== 'object' || !obj.path) {
        throw new Error('decodeSelectorJCB: Decoded object is not a valid Selector source');
    }
    return Selector.fromObject(obj);
}

/**
 * decodeJCB: General purpose alias for data decoding.
 */
export const decodeJCB = decodeDataJCB;

/**
 * encodeSafe: Selector -> JCB -> Base64 (Identity Transport)
 */
export function encodeSafe(val) {
    return toBase64(encodeSelectorJCB(val));
}

/**
 * decodeSafe: Base64 -> JCB -> Selector (Identity Transport)
 */
export function decodeSafe(base64) {
    return decodeSelectorJCB(fromBase64(base64));
}

export function decodeInfo(str) {
    if (!str) return {};
    try {
        if (str.trim().startsWith('{')) return JSON.parse(str);
        
        // Fallback for Base64 (legacy or char-safe)
        const decoded = fromBase64(str);
        if (decoded[0] === 0x06) return decodeDataJCB(decoded);
        return JSON.parse(new TextDecoder().decode(decoded));
    } catch (e) {
        console.error(`[CID] decodeInfo CRITICAL: Failed to parse metadata header: "${str}". Error: ${e.message}`);
        throw e;
    }
}

export function encodeInfo(info) {
    return JSON.stringify(info);
}

/**
 * vfs_hash256: SHA-256 helper.
 */
export async function vfs_hash256(bytes) {
  let cryptoImpl = globalThis.crypto;
  
  if (!cryptoImpl?.subtle) {
    try {
        const nodeCrypto = await import('node:crypto');
        cryptoImpl = nodeCrypto.webcrypto;
    } catch (e) {}
  }

  if (!cryptoImpl?.subtle) {
    throw new Error(
      "WebCrypto 'subtle' API is unavailable. This usually means the browser is blocking it because the connection is not secure (not HTTPS or localhost). " +
      "Mobile browsers REQUIRE HTTPS to access cryptographic features."
    );
  }

  const hash = await cryptoImpl.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(hash))
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
}

export const isString = (val) => typeof val === 'string' || Object.prototype.toString.call(val) === '[object String]';

export async function jcbHash(val) {
  // Protocol Rule: Hash the canonical binary representation (JCB) directly.
  return vfs_hash256(encodeSelectorJCB(val));
}

/**
 * getCID: Identity of terminal content.
 */
export async function getCID(data) {
  let bytes;
  if (data instanceof Uint8Array) {
      bytes = data;
  } else if (typeof data === 'string') {
      bytes = new TextEncoder().encode(data);
  } else {
      bytes = encodeDataJCB(data);
  }
  return vfs_hash256(bytes);
}

/**
 * getSelectorKey: Identity of a mesh address.
 */
export async function getSelectorKey(selector) {
  const s = normalizeSelector(selector);
  const json = s.toJSON();
  if (!json.output) delete json.output;
  return vfs_hash256(encodeSelectorJCB(Selector.fromObject(json)));
}
