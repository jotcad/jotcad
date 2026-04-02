import { SyncVFS, SYNC_VFS_STATUS, SYNC_VFS_OP } from './sync_vfs.js';

/**
 * Implementation of SyncVFS using Atomics and SharedArrayBuffer.
 * This client runs in the thread that needs synchronous access (e.g. WASM worker).
 */
export class SyncVFSAtomics extends SyncVFS {
  /**
   * @param {SharedArrayBuffer} sab The shared buffer for communication.
   */
  constructor(sab) {
    super();
    this.sab = sab;
    this.int32 = new Int32Array(sab);
    this.uint8 = new Uint8Array(sab);

    // Memory Map:
    // [0]: Status (SYNC_VFS_STATUS)
    // [1]: Operation (SYNC_VFS_OP)
    // [2]: Payload Length (JSON string of {path, parameters})
    // [3]: Data Length (For writes or result of reads)
    // [4...10]: Reserved
    // [40...]: Payload JSON string
    // [40 + payloadLength ...]: Binary Data
  }

  _request(op, path, parameters, data = null) {
    const payload = JSON.stringify({ path, parameters });
    const encoder = new TextEncoder();
    const encodedPayload = encoder.encode(payload);

    // 1. Write metadata
    this.int32[1] = op;
    this.int32[2] = encodedPayload.length;
    this.uint8.set(encodedPayload, 40);

    if (data) {
      this.int32[3] = data.length;
      this.uint8.set(data, 40 + encodedPayload.length);
    }

    // 2. Signal PENDING and Wait
    Atomics.store(this.int32, 0, SYNC_VFS_STATUS.PENDING);
    Atomics.notify(this.int32, 0); // Wake up host if it's waiting

    const result = Atomics.wait(this.int32, 0, SYNC_VFS_STATUS.PENDING);
    if (result === 'timed-out') throw new Error('SyncVFS request timed out');

    // 3. Check result
    const status = Atomics.load(this.int32, 0);
    if (status === SYNC_VFS_STATUS.ERROR) {
      throw new Error('SyncVFS remote error');
    }
    if (status === SYNC_VFS_STATUS.CLOSED) {
      throw new Error('SyncVFS connection closed');
    }

    // 4. Return data for READ
    if (op === SYNC_VFS_OP.READ) {
      const dataLen = this.int32[3];
      // Note: In a real implementation, we'd handle large files by streaming or multiple SAB round-trips.
      // For this abstraction, we copy from the shared buffer.
      const resultData = this.uint8.slice(
        40 + encodedPayload.length,
        40 + encodedPayload.length + dataLen
      );
      return resultData;
    }
  }

  read(path, parameters) {
    return this._request(SYNC_VFS_OP.READ, path, parameters);
  }

  write(path, parameters, data) {
    return this._request(SYNC_VFS_OP.WRITE, path, parameters, data);
  }
}

/**
 * The Server/Host component that fulfills SyncVFSAtomics requests.
 * Runs in the thread that owns the async VFS (Main thread or Node loop).
 */
export class SyncVFSServer {
  constructor(vfs, sab) {
    this.vfs = vfs;
    this.sab = sab;
    this.int32 = new Int32Array(sab);
    this.uint8 = new Uint8Array(sab);
    this.active = false;
  }

  /**
   * Starts the server loop.
   * In the browser, this must be called via setInterval or requestAnimationFrame
   * since we can't use Atomics.wait on the main thread.
   * In Node, we can use a worker thread or a polling loop.
   */
  async poll() {
    if (Atomics.load(this.int32, 0) !== SYNC_VFS_STATUS.PENDING) return;

    const op = this.int32[1];
    const payloadLen = this.int32[2];
    const payload = new TextDecoder().decode(
      this.uint8.slice(40, 40 + payloadLen)
    );
    const { path, parameters } = JSON.parse(payload);

    try {
      if (op === SYNC_VFS_OP.READ) {
        const stream = await this.vfs.read(path, parameters);
        const reader = stream.getReader();
        const chunks = [];
        let totalLen = 0;
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
          totalLen += value.length;
        }

        const combined = new Uint8Array(totalLen);
        let offset = 0;
        for (const chunk of chunks) {
          combined.set(chunk, offset);
          offset += chunk.length;
        }

        // Write back to SAB
        this.int32[3] = totalLen;
        this.uint8.set(combined, 40 + payloadLen);
        Atomics.store(this.int32, 0, SYNC_VFS_STATUS.SUCCESS);
      } else if (op === SYNC_VFS_OP.WRITE) {
        const dataLen = this.int32[3];
        const data = this.uint8.slice(
          40 + payloadLen,
          40 + payloadLen + dataLen
        );

        const stream = new ReadableStream({
          start(c) {
            c.enqueue(data);
            c.close();
          },
        });
        await this.vfs.write(path, parameters, stream);
        Atomics.store(this.int32, 0, SYNC_VFS_STATUS.SUCCESS);
      }
    } catch (err) {
      console.error('[SyncVFSServer] Error:', err);
      Atomics.store(this.int32, 0, SYNC_VFS_STATUS.ERROR);
    } finally {
      Atomics.notify(this.int32, 0);
    }
  }

  start(interval = 10) {
    this.active = true;
    const loop = async () => {
      if (!this.active) return;
      await this.poll();
      setTimeout(loop, interval);
    };
    loop();
  }

  stop() {
    this.active = false;
  }
}
