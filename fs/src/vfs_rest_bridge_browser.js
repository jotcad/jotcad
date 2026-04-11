import { MeshLinkBase } from './mesh_link.js';

/**
 * MeshLink for Browser environments.
 * Uses native fetch.
 */
export class MeshLink extends MeshLinkBase {
  constructor(vfs, neighbors = []) {
    super(vfs, neighbors, { 
      fetch: globalThis.fetch.bind(globalThis) 
    });
  }
}
