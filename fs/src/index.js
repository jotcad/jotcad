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
  Selector,
  encodeJCB, 
  decodeJCB,
  WebReadableStream
} from './vfs_core.js';

export { MeshLink } from './mesh_link.js';
export { registerVFSRoutes } from './vfs_rest_server.js';
export { LegacyNode as Node, In, Out, Watch } from './node.js';
export { log, warn, error } from './log.js';
