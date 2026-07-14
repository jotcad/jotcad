import http from 'node:http';
import { VFS, MemoryStorage, MeshLink, registerVFSRoutes, Selector } from '../src/index.js';

/**
 * Helper to create a protocol-compliant VFS result from raw data.
 */
export function vfsResult(data, metadata = {}) {
    let bytes, encoding = metadata.encoding || 'bytes';
    if (data instanceof Uint8Array) {
        bytes = data;
    } else if (typeof data === 'string') {
        bytes = new TextEncoder().encode(data);
        encoding = metadata.encoding || 'string';
    } else if (data === null || data === undefined) {
        bytes = new Uint8Array();
        encoding = metadata.encoding || 'null';
    } else {
        bytes = new TextEncoder().encode(JSON.stringify(data));
        encoding = metadata.encoding || 'json';
    }
    return {
        stream: new ReadableStream({
            start(controller) {
                controller.enqueue(bytes);
                controller.close();
            }
        }),
        metadata: { state: 'AVAILABLE', ...metadata, encoding }
    };
}

/**
 * Creates a mock provider that returns a fixed value wrapped in a VFS result.
 */
export const mockOp = (val, metadata) => async () => vfsResult(val, metadata);

/**
 * TestVFSNode: A fully functional VFS node for integration testing.
 */
export class TestVFSNode {
  constructor(id, port, neighbors = []) {
    this.id = id;
    this.port = port;
    this.neighbors = neighbors;
    this.vfs = new VFS({ id, storage: new MemoryStorage() });
    this.server = http.createServer();
    this.meshLink = null;
    this.sockets = new Set();

    this.server.on('connection', (socket) => {
      this.sockets.add(socket);
      socket.on('close', () => {
        this.sockets.delete(socket);
      });
    });
  }

  async start() {
    await this.vfs.init();
    
    return new Promise((resolve, reject) => {
      const onError = (err) => {
        this.server.off('listening', onListening);
        reject(err);
      };
      const onListening = async () => {
        this.server.off('error', onError);
        this.port = this.server.address().port;
        this.meshLink = new MeshLink(this.vfs, this.neighbors, {
          localUrl: `http://localhost:${this.port}`
        });
        this.cleanupRoutes = registerVFSRoutes(this.vfs, this.server, '', this.meshLink);
        try {
          await this.meshLink.start();
          resolve();
        } catch (err) {
          reject(err);
        }
      };
      this.server.once('error', onError);
      this.server.once('listening', onListening);
      this.server.listen(this.port || 0, 'localhost');
    });
  }

  async stop() {
    if (this.cleanupRoutes) this.cleanupRoutes();
    
    const serverClosePromise = new Promise((resolve) => this.server.close(resolve));
    for (const socket of this.sockets) {
      try { socket.destroy(); } catch (e) {}
    }
    this.sockets.clear();
    await serverClosePromise;

    if (this.meshLink) {
      await Promise.race([
        this.meshLink.stop(),
        new Promise((resolve) => setTimeout(resolve, 1000))
      ]);
    }
  }

  registerProvider(path, handler, schema) {
    this.vfs.registerProvider(path, handler, { schema });
  }
}
