import { Volume, createFsFromVolume } from 'memfs';
import fs from 'node:fs';
import fsPromises from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import { pipeline } from 'node:stream/promises';

/**
 * In-memory sandbox using memfs.
 * Best for WASM or in-process native bindings.
 */
export class MemSandbox {
  constructor() {
    this.vol = new Volume();
    this.fs = createFsFromVolume(this.vol);
  }

  async put(filename, content) {
    const data = await this._toUint8Array(content);
    const dirname = path.dirname(filename);
    if (dirname !== '.' && dirname !== '/') {
      this.fs.mkdirSync(dirname, { recursive: true });
    }
    this.fs.writeFileSync(filename, data);
  }

  async get(filename) {
    if (!this.fs.existsSync(filename)) return null;
    const data = this.fs.readFileSync(filename);
    return new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(data.buffer, data.byteOffset, data.byteLength));
        controller.close();
      }
    });
  }

  async _toUint8Array(content) {
    if (content instanceof Uint8Array) return content;
    if (typeof content === 'string') return new TextEncoder().encode(content);
    
    // Handle streams
    const reader = content.getReader ? content.getReader() : null;
    if (reader) {
      const chunks = [];
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }
      const totalLength = chunks.reduce((acc, c) => acc + c.length, 0);
      const result = new Uint8Array(totalLength);
      let offset = 0;
      for (const chunk of chunks) {
        result.set(chunk, offset);
        offset += chunk.length;
      }
      return result;
    }
    
    // Handle Node streams
    if (content.on) {
        const chunks = [];
        for await (const chunk of content) chunks.push(chunk);
        return new Uint8Array(Buffer.concat(chunks).buffer);
    }

    return new TextEncoder().encode(JSON.stringify(content));
  }

  close() {
    this.vol.reset();
  }
}

/**
 * Disk-based sandbox using a temporary directory.
 * Best for standalone C++ binaries/child processes.
 */
export class DiskSandbox {
  constructor(prefix = 'jotcad-sandbox-') {
    this.root = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  }

  async put(filename, content) {
    const fullPath = path.join(this.root, filename);
    await fsPromises.mkdir(path.dirname(fullPath), { recursive: true });
    
    const out = fs.createWriteStream(fullPath);
    if (content.pipe) {
        await pipeline(content, out);
    } else if (content.getReader) {
        // Convert web stream to node stream for pipeline
        const nodeStream = fs.createReadStream(null, { fd: -1 }); // Placeholder?
        // Actually simpler to just write the Uint8Array
        await fsPromises.writeFile(fullPath, await this._toUint8Array(content));
    } else {
        await fsPromises.writeFile(fullPath, await this._toUint8Array(content));
    }
  }

  async get(filename) {
    const fullPath = path.join(this.root, filename);
    if (!fs.existsSync(fullPath)) return null;
    return fs.createReadStream(fullPath);
  }

  async _toUint8Array(content) {
    // Re-use logic from MemSandbox or keep it simple for disk
    if (typeof content === 'string') return content;
    if (content instanceof Uint8Array) return content;
    
    const reader = content.getReader ? content.getReader() : null;
    if (reader) {
        const chunks = [];
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
        }
        return Buffer.concat(chunks);
    }
    return JSON.stringify(content);
  }

  async close() {
    await fsPromises.rm(this.root, { recursive: true, force: true });
  }
}
