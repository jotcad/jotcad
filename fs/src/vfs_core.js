/**
 * VFS Core: Modular re-exports
 */
export { 
  normalizeSelector, 
  getCID, 
  getSelectorKey, 
  encodeSafe, 
  decodeSafe, 
  encodeJCB, 
  decodeJCB 
} from './cid.js';

export { EventEmitter } from './event_emitter.js';
export { MemoryStorage } from './memory_storage.js';
export { VFS, VFSClosedError } from './vfs.js';

export const WebReadableStream = globalThis.ReadableStream;
