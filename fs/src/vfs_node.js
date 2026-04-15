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
  const json = JSON.stringify(s);
  return crypto.createHash('sha256').update(json).digest('hex');
};

export class DiskStorage {
  constructor(root) {
    this.root = root;
    if (!fs.existsSync(root)) fs.mkdirSync(root, { recursive: true });
  }
  async get(cid) {
    const dataFile = path.join(this.root, `${cid}.data`);
    if (!fs.existsSync(dataFile)) return null;
    const nodeStream = fs.createReadStream(dataFile);
    return Readable.toWeb(nodeStream);
  }
  async set(cid, data, info) {
    const metaFile = path.join(this.root, `${cid}.meta`);
    const dataFile = path.join(this.root, `${cid}.data`);

    if (!data) return;

    let bytesWritten = 0;
    const expectedSize = info?.size;

    try {
      if (typeof data.pipeTo === 'function') {
        const out = fs.createWriteStream(dataFile);
        const writable = new WritableStream({
          write(chunk) {
            bytesWritten += chunk.length;
            return new Promise((resolve, reject) => {
              if (!out.write(chunk)) out.once('drain', resolve);
              else resolve();
            });
          },
          close() {
            return new Promise((resolve) => {
              out.end(resolve);
            });
          },
          abort(err) {
            out.destroy(err);
          },
        });
        await data.pipeTo(writable);
      } else if (typeof data.pipe === 'function') {
        const out = fs.createWriteStream(dataFile);
        data.on('data', (chunk) => {
          bytesWritten += chunk.length;
        });
        await pipeline(data, out);
      } else {
        const buffer = typeof data === 'string' ? Buffer.from(data) : data;
        bytesWritten = buffer.length;
        await fsPromises.writeFile(dataFile, buffer);
      }

      // Verify size
      if (expectedSize !== undefined && bytesWritten !== expectedSize) {
        throw new Error(
          `Disk write aborted or corrupted: Expected ${expectedSize} bytes, got ${bytesWritten}`
        );
      }

      // Only write meta if data was successful
      await fsPromises.writeFile(metaFile, JSON.stringify(info, null, 2));
    } catch (err) {
      // Cleanup partial files on error
      await fsPromises.unlink(dataFile).catch(() => {});
      await fsPromises.unlink(metaFile).catch(() => {});
      throw err;
    }
  }
  async has(cid) {
    return fs.existsSync(path.join(this.root, `${cid}.data`));
  }
  async delete(cid) {
    try {
      await fsPromises.unlink(path.join(this.root, `${cid}.meta`));
      await fsPromises.unlink(path.join(this.root, `${cid}.data`));
    } catch (e) {}
  }
  async close() {}

  async *iterateMeta() {
    if (!fs.existsSync(this.root)) return;
    const files = await fsPromises.readdir(this.root);
    for (const file of files) {
      if (file.endsWith('.meta')) {
        const cid = file.slice(0, -5);
        try {
          const content = await fsPromises.readFile(
            path.join(this.root, file),
            'utf8'
          );
          const info = JSON.parse(content);
          yield { cid, info };
        } catch (e) {}
      }
    }
  }
}

export class VFS extends CoreVFS {
  constructor(options = {}) {
    super({ getCID, ...options });
    this.initialized = false;
  }

  async init() {
    if (this.initialized) return;
    this.initialized = true;

    if (this.storage instanceof DiskStorage) {
      console.log(
        `[VFS ${this.id}] EPHEMERAL WIPE: Cleaning storage directory: ${this.storage.root}`
      );
      try {
        const files = await fsPromises.readdir(this.storage.root);
        for (const file of files) {
          if (file.endsWith('.meta') || file.endsWith('.data')) {
            await fsPromises.unlink(path.join(this.storage.root, file));
          }
        }
      } catch (e) {}
    }
  }
}
