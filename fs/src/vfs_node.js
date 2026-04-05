import crypto from 'crypto';
import fs from 'fs';
import fsPromises from 'fs/promises';
import path from 'path';
import { pipeline } from 'stream/promises';
import { VFS as CoreVFS } from './vfs_core.js';

export { VFSClosedError } from './vfs_core.js';

export const getCID = async (selector) => {
  const { path, parameters = {} } = selector;
  if (!path) throw new Error('Selector must have a path');
  
  const sortedParams = Object.keys(parameters)
    .sort()
    .reduce((acc, key) => {
      acc[key] = parameters[key];
      return acc;
    }, {});
    
  const hash = crypto.createHash('sha256');
  hash.update(path + JSON.stringify(sortedParams));
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
    if (this.storage instanceof DiskStorage) {
      const files = await fsPromises.readdir(this.storage.root);
      for (const file of files) {
        if (file.endsWith('.meta')) {
          const cid = file.slice(0, -5);
          const metaContent = await fsPromises.readFile(
            path.join(this.storage.root, file),
            'utf-8'
          );
          try {
            const info = JSON.parse(metaContent);
            this.states.set(cid, { ...info, state: 'AVAILABLE' });
          } catch (e) {
            console.warn(`[VFS] Failed to parse meta for ${cid}`, e);
          }
        }
      }
    }
  }
}
