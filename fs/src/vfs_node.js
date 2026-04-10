import crypto from 'crypto';
import fs from 'fs';
import fsPromises from 'fs/promises';
import path from 'path';
import { pipeline } from 'stream/promises';
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
    return fs.createReadStream(dataFile);
  }
  async set(cid, stream, info) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);
    await fsPromises.writeFile(metaFile, JSON.stringify(info, null, 2));
    const out = fs.createWriteStream(dataFile);
    await pipeline(stream, out);
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
      const files = await fsPromises.readdir(this.storage.root);
      for (const file of files) {
        if (file.endsWith('.meta')) {
          const cid = file.slice(0, -5);
          try {
            const metaContent = await fsPromises.readFile(
              path.join(this.storage.root, file),
              'utf-8'
            );
            const info = JSON.parse(metaContent);
            // Crucial: ensure sources exists and is an array
            const sanitizedInfo = {
                path: info.path,
                parameters: info.parameters || {},
                sources: Array.isArray(info.sources) ? info.sources : [],
                state: 'AVAILABLE'
            };
            this.states.set(cid, sanitizedInfo);
          } catch (e) {
            console.warn(`[VFS] Failed to parse meta for ${cid}`, e);
          }
        }
      }
    }
  }
}
