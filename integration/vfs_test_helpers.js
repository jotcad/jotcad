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
  const targets = targetIds.map(id => id === 'geo-ops-node' ? 'geo-' : id);

  const matchProvider = (provider, target) => {
    if (target === 'geo-') {
      return provider.startsWith('geo-');
    }
    return provider === target;
  };

  if (vfs.mesh && vfs.mesh.session) {
    const startTime = Date.now();
    const { decodeRecord } = await import('../fs/src/mesh/mesh_link.js');
    
    while (Date.now() - startTime < timeout) {
      try {
        const receiver = await vfs.mesh.session.get('jot/vfs/catalog', { 
          timeout: Math.min(2000, timeout - (Date.now() - startTime)),
          target: 1,
          consolidation: 1
        });
        if (receiver) {
          for await (const reply of receiver) {
            const result = reply.result();
            if (result instanceof Error || result?.constructor?.name === 'ReplyError') {
              continue;
            }
            
            const sample = result;
            const recordBytes = sample.payload().toBytes();
            const decoded = decodeRecord(recordBytes);
            if (!decoded) {
              continue;
            }
            
            const { header, payload } = decoded;
            if (header.status === 200) {
              const catalogPayload = JSON.parse(new TextDecoder().decode(payload));
              const provider = catalogPayload.provider;
              if (vfs.mesh && provider !== vfs.id) {
                vfs.mesh.updateCatalog(catalogPayload);
              }
              for (let i = 0; i < targets.length; i++) {
                if (matchProvider(provider, targets[i])) {
                  targets.splice(i, 1);
                  i--;
                }
              }
            }
          }
        }
      } catch (err) {
        console.error(`[waitForMeshNodes ${vfs.id}] query error:`, err);
      }
      if (targets.length === 0) {
        return; // All target nodes found!
      }
      await new Promise(r => setTimeout(r, 200));
    }
    throw new Error(`waitForMeshNodes: Target nodes [${targetIds.join(', ')}] not found within ${timeout}ms`);
  }

  const seen = new Set([vfs.id]);
  const hasAllTargets = () => {
    return targets.every(target => {
      return [...seen].some(provider => matchProvider(provider, target));
    });
  };

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
            if (hasAllTargets()) {
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
        const missing = targetIds.filter((id, idx) => {
            const target = targets[idx];
            return ![...seen].some(provider => matchProvider(provider, target));
        });
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
    if (hasAllTargets()) {
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
