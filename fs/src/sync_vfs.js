/**
 * Abstraction layer for Synchronous VFS access.
 * This allows C++/WASM code to perform blocking I/O against the distributed blackboard.
 */
export class SyncVFS {
  /**
   * Synchronously reads a file. Blocks the calling thread.
   * @param {string} path 
   * @param {Object} parameters 
   * @returns {Uint8Array}
   */
  read(path, parameters) {
    throw new Error('SyncVFS.read() not implemented');
  }

  /**
   * Synchronously writes a file. Blocks the calling thread.
   * @param {string} path 
   * @param {Object} parameters 
   * @param {Uint8Array} data 
   */
  write(path, parameters, data) {
    throw new Error('SyncVFS.write() not implemented');
  }
}

/**
 * Shared memory constants for the Atomics-based bridge.
 */
export const SYNC_VFS_STATUS = {
  IDLE: 0,
  PENDING: 1,
  SUCCESS: 2,
  ERROR: 3,
  CLOSED: 4
};

export const SYNC_VFS_OP = {
  READ: 1,
  WRITE: 2,
  STATUS: 3
};
