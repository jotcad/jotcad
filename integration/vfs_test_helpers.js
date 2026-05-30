import http from 'node:http';
import { VFS, MemoryStorage, MeshLink, registerVFSRoutes, Selector, Connection } from '../fs/src/index.js';

/**
 * MockConnection: A minimal implementation of the Connection interface for testing.
 * Inherits from the formal Connection class to ensure protocol compliance.
 */
export class MockConnection extends Connection {
  constructor(neighborId, onSend = () => {}) {
    super(neighborId);
    this.reachability = 'DIRECT';
    this.onSend = onSend;
  }

  getProtocol() {
    return 'mock';
  }

  async send(req) {
    return this.onSend(req);
  }
}

/**
 * waitForMeshNodes: Proactively waits for a list of node IDs to be visible in the mesh.
 * This helper subscribes to 'sys/topo' and waits until it sees the target nodes either
 * as direct peers or as reported neighbors in topology updates.
 */
export async function waitForMeshNodes(vfs, targetIds, timeout = 15000) {
  const targetSet = new Set(targetIds);
  const seen = new Set([vfs.id]);

  return new Promise(async (resolve, reject) => {
    let unsubscribe = () => {};

    const check = (selector, payload) => {
        if (selector.path === 'sys/topo') {
            const prevSize = seen.size;
            // Learn from the sender (direct)
            if (payload && payload.id) seen.add(payload.id);
            
            // Learn from the sender's reported neighbors (Multi-hop)
            if (payload && Array.isArray(payload.neighbors)) {
                for (const neighbor of payload.neighbors) {
                    const neighborId = neighbor.id || neighbor;
                    seen.add(neighborId);
                }
            }

            // Sync with local mesh state
            if (vfs.mesh) {
                for (const peerId of vfs.mesh.peers.keys()) seen.add(peerId);
            }
            
            if (seen.size > prevSize) {
                console.log(`[waitForMeshNodes ${vfs.id}] New nodes discovered! Current view: [${[...seen].join(', ')}]`);
            }

            // Check if we have everything
            if ([...targetSet].every(id => seen.has(id))) {
                cleanup();
                resolve();
            }
        }
    };

    const cleanup = () => {
        clearTimeout(timer);
        unsubscribe();
    };

    const timer = setTimeout(() => {
        cleanup();
        const missing = [...targetSet].filter(id => !seen.has(id));
        reject(new Error(`waitForMeshNodes timeout after ${timeout}ms. \n  Missing: [${missing.join(', ')}]\n  Discovered: [${[...seen].join(', ')}]`));
    }, timeout);

    // 1. Subscribe WITH callback (atomic registration)
    try {
        unsubscribe = await vfs.subscribe(new Selector('sys/topo'), Date.now() + timeout, [], check);
    } catch (err) {
        cleanup();
        reject(err);
        return;
    }
    
    // 2. Initial check of current state (in case we're already connected)
    if (vfs.mesh) {
        for (const peerId of vfs.mesh.peers.keys()) seen.add(peerId);
    }
    if ([...targetSet].every(id => seen.has(id))) {
        cleanup();
        resolve();
    }
  });
}

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

    const stream = new ReadableStream({
        start(controller) {
            controller.enqueue(bytes);
            controller.close();
        }
    });

    return { stream, metadata: { ...metadata, encoding } };
}
