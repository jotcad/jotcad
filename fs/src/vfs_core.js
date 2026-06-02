/**
 * VFS Core: Modular re-exports
 */
export { 
  normalizeSelector, 
  Selector,
  getCID, 
  getSelectorKey, 
  encodeSafe, 
  decodeSafe, 
  encodeJCB, 
  decodeJCB,
  encodeInfo,
  decodeInfo
} from './cid.js';

export { EventEmitter } from './event_emitter.js';
export { MemoryStorage } from './memory_storage.js';
export { VFS, VFSClosedError } from './vfs.js';

export const WebReadableStream = globalThis.ReadableStream;
