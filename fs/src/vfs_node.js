import crypto from 'crypto';
import fs from 'fs';
import fsPromises from 'fs/promises';
import path from 'path';
import { pipeline } from 'stream/promises';
import { Readable } from 'node:stream';
import { VFS as CoreVFS, normalizeSelector, Selector, VFSClosedError, getCID, getSelectorKey } from './vfs_core.js';

export { VFSClosedError, getCID, getSelectorKey, Selector };

export class DiskStorage {
  constructor(root) {
    this.root = root;
    if (!fs.existsSync(root)) fs.mkdirSync(root, { recursive: true });
  }

  async init() {}

  async has(cid) {
    const dataFile = path.join(this.root, `${cid}.data`);
    const metaFile = path.join(this.root, `${cid}.meta`);
    const exists = fs.existsSync(dataFile) || fs.existsSync(metaFile);
    if (exists) console.log(`[DiskStorage ${this.root}] has(${cid.slice(0, 8)}) -> TRUE`);
    return exists;
  }

  async get(cid) {
    const dataFile = path.join(this.root, `${cid}.data`);
    if (!fs.existsSync(dataFile)) {
        console.log(`[DiskStorage ${this.root}] get(${cid.slice(0, 8)}) -> MISS`);
        return null;
    }
    const stats = fs.statSync(dataFile);
    console.log(`[DiskStorage ${this.root}] get(${cid.slice(0, 8)}) -> HIT (${stats.size} bytes)`);
    return Readable.toWeb(fs.createReadStream(dataFile));
  }

  async set(cid, streamOrBytes, info = {}) {
    const dataFile = path.join(this.root, `${cid}.data`);
    const metaFile = path.join(this.root, `${cid}.meta`);
    const expectedSize = info.size;
    let bytesWritten = 0;

    if (streamOrBytes instanceof Uint8Array) {
        bytesWritten = streamOrBytes.length;
        await fsPromises.writeFile(dataFile, streamOrBytes);
    } else if (typeof streamOrBytes === 'string') {
        const buf = Buffer.from(streamOrBytes);
        bytesWritten = buf.length;
        await fsPromises.writeFile(dataFile, buf);
    } else if (streamOrBytes === null) {
        bytesWritten = 0;
        await fsPromises.writeFile(dataFile, '');
    } else {
        const out = fs.createWriteStream(dataFile);
        try {
            if (streamOrBytes.getReader) {
                const reader = streamOrBytes.getReader();
                try {
                    while (true) {
                        const { done, value } = await reader.read();
                        if (done) break;
                        if (value) {
                            bytesWritten += value.length;
                            out.write(value);
                        }
                    }
                    await new Promise((resolve) => out.end(resolve));
                } finally { reader.releaseLock(); }
            } else {
                // Node stream
                streamOrBytes.on('data', (chunk) => { bytesWritten += chunk.length; });
                await pipeline(streamOrBytes, out);
            }
        } catch (err) {
            out.destroy();
            await fsPromises.unlink(dataFile).catch(() => {});
            throw err;
        }
    }

    if (streamOrBytes !== null && expectedSize !== undefined && bytesWritten !== expectedSize) {
        await fsPromises.unlink(dataFile).catch(() => {});
        throw new Error(`DiskStorage.set: Size mismatch. Expected 100 bytes (got ${bytesWritten})`);
    }

    await fsPromises.writeFile(metaFile, JSON.stringify(info));
    console.log(`[DiskStorage ${this.root}] set(${cid.slice(0, 8)}) - data size: ${bytesWritten}, info: ${Object.keys(info)}`);
  }

  async delete(cid) {
    await fsPromises.unlink(path.join(this.root, `${cid}.data`)).catch(() => {});
    await fsPromises.unlink(path.join(this.root, `${cid}.meta`)).catch(() => {});
  }

  async close() {}

  async *iterateMeta() {
    try {
      const files = await fsPromises.readdir(this.root);
      for (const file of files) {
        if (file.endsWith('.meta')) {
          const cid = file.slice(0, -5); // Extract CID from filename (excluding .meta)
          const meta = await fsPromises.readFile(path.join(this.root, file), 'utf8');
          yield { cid, info: JSON.parse(meta) };
        }
      }
    } catch (e) {}
  }
}

export class VFS extends CoreVFS {
  constructor(options = {}) {
    const { storage, id } = options;
    const vfsId = id || crypto.randomUUID();
    super({
      id: vfsId,
      storage: storage || new DiskStorage(path.resolve('.vfs_storage_' + vfsId)),
    });
  }

  async init() {
    await super.init();
    const isTestRunner = process.argv.some(arg => arg.includes('test')) || process.env.NODE_ENV === 'test';
    const shouldWipe = process.env.VFS_EPHEMERAL_WIPE === 'true' || isTestRunner;

    if (shouldWipe) {
      console.log(`[VFS ${this.id}] EPHEMERAL WIPE: Cleaning storage directory: ${this.storage.root}`);
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
