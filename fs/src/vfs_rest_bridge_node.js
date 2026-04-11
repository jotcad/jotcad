import { MeshLinkBase } from './mesh_link.js';

/**
 * MeshLink for Node.js environments.
 */
export class MeshLink extends MeshLinkBase {
  constructor(vfs, neighbors = [], options = {}) {
    super(vfs, neighbors, { 
        fetch: globalThis.fetch, 
        localUrl: options.localUrl 
    });
  }
}
