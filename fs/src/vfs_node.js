import crypto from 'crypto';
import fs from 'fs';
import fsPromises from 'fs/promises';
import path from 'path';
import { pipeline } from 'stream/promises';
import { Readable } from 'node:stream';
import { VFS as CoreVFS, normalizeSelector } from './vfs_core.js';

export { VFSClosedError } from './vfs_core.js';

export const getCID = async (selector) => {
  const s = normalizeSelector(selector.path, selector.parameters);
  if (!s.path) throw new Error('Selector must have a path');
  
  const hash = crypto.createHash('sha256');
  // s.parameters is already recursively sorted and normalized
  hash.update(s.path + JSON.stringify(s.parameters));
  return hash.digest('hex');
};

export class DiskStorage {
  constructor(root) {
    this.root = root;
    if (!fs.existsSync(this.root)) {
      fs.mkdirSync(this.root, { recursive: true });
    }
  }
  async get(cid) {
    const dataFile = path.join(this.root, `${cid}.data`);
    if (!fs.existsSync(dataFile)) return null;
    const nodeStream = fs.createReadStream(dataFile);
    return Readable.toWeb(nodeStream);
  }
  async set(cid, stream, info) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);
    await fsPromises.writeFile(metaFile, JSON.stringify(info, null, 2));
    
    // Handle both Node and Web Streams
    if (typeof stream.pipeTo === 'function') {
        const out = fs.createWriteStream(dataFile);
        await stream.pipeTo(new WritableStream({
            write(chunk) { out.write(chunk); },
            close() { out.end(); }
        }));
    } else {
        const out = fs.createWriteStream(dataFile);
        await pipeline(stream, out);
    }
  }
  async has(cid) {
    return fs.existsSync(path.join(this.root, `${cid}.data`));
  }
  async delete(cid) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);
    if (fs.existsSync(metaFile)) await fsPromises.unlink(metaFile);
    if (fs.existsSync(dataFile)) await fsPromises.unlink(dataFile);
  }
  async close() {}
}

export class VFS extends CoreVFS {
  constructor(options = {}) {
    super({ getCID, ...options });
  }

  async init() {
    if (this.initialized) return;
    this.initialized = true;
    
    if (this.storage instanceof DiskStorage) {
      console.log(`[VFS ${this.id}] EPHEMERAL WIPE: Cleaning storage directory: ${this.storage.root}`);
      try {
        const files = await fsPromises.readdir(this.storage.root);
        for (const file of files) {
          if (file.endsWith('.meta') || file.endsWith('.data')) {
            await fsPromises.unlink(path.join(this.storage.root, file));
          }
        }
        console.log(`[VFS ${this.id}] Successfully wiped storage. Starting with a Clean Slate.`);
      } catch (e) {
        console.error(`[VFS ${this.id}] ERROR: Failed to wipe storage directory.`, e);
      }
    }
  }
}
