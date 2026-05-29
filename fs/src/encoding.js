/**
 * toBase64: Standard Base64 encoding for Uint8Array.
 * Supports both Node.js (Buffer) and Browser (btoa) environments.
 */
export function toBase64(bytes) {
  if (typeof Buffer !== 'undefined') return Buffer.from(bytes).toString('base64');
  let binary = '';
  const len = (bytes.byteLength !== undefined) ? bytes.byteLength : bytes.length;
  for (let i = 0; i < len; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

/**
 * fromBase64: Standard Base64 decoding to Uint8Array.
 * Supports both Node.js (Buffer) and Browser (atob) environments.
 */
export function fromBase64(base64) {
  if (typeof Buffer !== 'undefined') return new Uint8Array(Buffer.from(base64, 'base64'));
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}
