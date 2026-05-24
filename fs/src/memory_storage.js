import { log } from './log.js';

export class MemoryStorage {
  constructor(vfsId) {
    this.vfsId = vfsId;
    this.data = new Map();
    this.info = new Map();
  }
  async init() {}
  async has(cid) { 
    const result = this.data.has(cid) || this.info.has(cid);
    if (result) log(`[MemoryStorage ${this.vfsId}] has(${cid.slice(0, 8)}) -> TRUE`);
    return result;
  }
  async get(cid) {
    const bytes = this.data.get(cid);
    if (bytes) log(`[MemoryStorage ${this.vfsId}] get(${cid.slice(0, 8)}) -> HIT (${bytes.length} bytes)`);
    else log(`[MemoryStorage ${this.vfsId}] get(${cid.slice(0, 8)}) -> MISS`);
    return bytes ? new ReadableStream({ start(c) { c.enqueue(bytes); c.close(); } }) : null;
  }
  async getMeta(cid) {
    return this.info.get(cid) || null;
  }
  async set(cid, streamOrBytes, info = {}) {
    let bytes;
    log(`[MemoryStorage ${this.vfsId}] set(${cid.slice(0, 8)}) called with type: ${typeof streamOrBytes}, isUint8: ${streamOrBytes instanceof Uint8Array}`);
    if (streamOrBytes instanceof Uint8Array) {
      bytes = streamOrBytes;
    } else if (typeof streamOrBytes === 'string') {
      bytes = new TextEncoder().encode(streamOrBytes);
    } else if (streamOrBytes === null) {
      bytes = new Uint8Array();
    } else if (streamOrBytes && typeof streamOrBytes.getReader === 'function') {
      const chunks = [];
      const reader = streamOrBytes.getReader();
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          if (value) chunks.push(value);
        }
      } finally { reader.releaseLock(); }
      const len = chunks.reduce((acc, c) => acc + c.length, 0);
      bytes = new Uint8Array(len);
      let offset = 0;
      for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    } else {
        bytes = new TextEncoder().encode(JSON.stringify(streamOrBytes));
    }

    const expectedSize = info.size;
    if (streamOrBytes !== null && expectedSize !== undefined && bytes.length !== expectedSize) {
        log(`[MemoryStorage ${this.vfsId}] Size mismatch: expected ${expectedSize}, got ${bytes.length}`);
        throw new Error(`MemoryStorage.set: Size mismatch. Expected ${expectedSize} bytes, got ${bytes.length}`);
    }

    this.data.set(cid, bytes);
    this.info.set(cid, info);
    log(`[MemoryStorage ${this.vfsId}] set(${cid.slice(0, 8)}) - final size: ${bytes.length}`);
  }
  async delete(cid) { this.data.delete(cid); this.info.delete(cid); }
  async close() { this.data.clear(); this.info.clear(); }
  async *iterateMeta() {
    for (const [key, info] of this.info.entries()) { yield { cid: key, info }; }
  }
}
