/**
 * toBase64: Standard Base64 encoding for Uint8Array.
 */
function toBase64(bytes) {
  if (globalThis.Buffer) return Buffer.from(bytes).toString('base64');
  let binary = '';
  for (let i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
  return globalThis.btoa(binary);
}

/**
 * fromBase64: Standard Base64 decoding to Uint8Array.
 */
function fromBase64(base64) {
  if (globalThis.Buffer) return new Uint8Array(Buffer.from(base64, 'base64'));
  const binary = globalThis.atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

/**
 * encodeJCB: Deterministic Binary Encoding (JotCAD Canonical Binary)
 * Handles recursive sorting of object keys internally.
 */
export function encodeJCB(val) {
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
      const keys = Object.keys(v).sort();
      const header = new Uint8Array(5);
      header[0] = 0x06;
      const view = new DataView(header.buffer);
      view.setUint32(1, keys.length, false);
      chunks.push(header);
      for (const k of keys) {
        walk(k);
        walk(v[k]);
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
 * encodeSafe: json -> JCB -> Base64
 */
export function encodeSafe(val) {
    return toBase64(encodeJCB(val));
}

/**
 * decodeSafe: Base64 -> JCB -> json
 */
export function decodeSafe(base64) {
    return decodeJCB(fromBase64(base64));
}

/**
 * decodeJCB: Decodes JCB bytes back into JS values.
 */
export function decodeJCB(bytes) {
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

export async function vfs_hash256(bytes) {
  let cryptoImpl = globalThis.crypto;
  
  if (!cryptoImpl?.subtle) {
    // Dynamic import for Node.js to avoid breaking browser bundles
    const nodeCrypto = await import('node:crypto');
    cryptoImpl = nodeCrypto.webcrypto;
  }

  const hash = await cryptoImpl.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(hash))
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
}

export async function jcbHash(val) {
  const safe = encodeSafe(val);
  return vfs_hash256(new TextEncoder().encode(safe));
}

/**
 * getCID: Identity of terminal content.
 * 
 * JCB (JotCAD Canonical Binary) is used to produce a stable, 
 * deterministic representation of structured data across JS and C++.
 * 
 * - Identity (CID): Derived by hashing the Safe-JCB string (Base64-encoded JCB).
 * - Storage: Disk content remains raw (e.g., human-readable JSON).
 * - Transport: Safe-JCB carrier strings are used when JSON-safety is required.
 */
export async function getCID(data) {
  let bytes;
  if (data instanceof Uint8Array) {
      bytes = data;
  } else if (typeof data === 'string') {
      bytes = new TextEncoder().encode(data);
  } else {
      // For structured data, the CID is the hash of the Safe-JCB string.
      // Safe-JCB = Base64(JCB binary).
      const safe = encodeSafe(data);
      bytes = new TextEncoder().encode(safe);
  }
  return vfs_hash256(bytes);
}

/**
 * getSelectorKey: Identity of a mesh address.
 * Uses deterministic JCB to ensure that {path:'a', parameters:{b:1, c:2}}
 * and {path:'a', parameters:{c:2, b:1}} produce the same key.
 */
export async function getSelectorKey(selector) {
  const s = normalizeSelector(selector);
  // Match C++ behavior: only include 'output' in hash if it's non-empty
  if (!s.output) delete s.output;
  // Ensure parameters are sorted by using encodeJCB which handles key ordering.
  return await jcbHash(s);
}

/**
 * normalizeSelector: Internal helper to ensure API inputs are objects.
 */
export function normalizeSelector(pathOrSelector, parameters = {}) {
  if (pathOrSelector && typeof pathOrSelector === 'object' && pathOrSelector.path !== undefined) {
    return { ...pathOrSelector };
  } else if (typeof pathOrSelector === 'string') {
    return { path: pathOrSelector, parameters: parameters || {}, output: '$out' };
  } else {
    // If no path, it's just parameters (anonymous)
    const s = { path: '', parameters: pathOrSelector || {}, output: '' };
    // Check if it's already a normalized selector-like object without a path
    if (pathOrSelector && pathOrSelector.parameters) {
        s.parameters = pathOrSelector.parameters;
        if (pathOrSelector.output) s.output = pathOrSelector.output;
    }
    return s;
  }
}
