export { 
  VFS, 
  DiskStorage, 
  getCID, 
  getSelectorKey, 
  VFSClosedError 
} from './vfs_node.js';

export { 
  MemoryStorage, 
  normalizeSelector, 
  encodeJCB, 
  decodeJCB 
} from './vfs_core.js';

export { MeshLink } from './mesh_link.js';
export { registerVFSRoutes } from './vfs_rest_server.js';
export { Node, In, Out, Watch } from './node.js';
