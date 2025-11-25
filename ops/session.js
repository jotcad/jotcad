// ops/Session.js

import path from 'node:path';

/**
 * @typedef {object} SessionPaths
 * @property {string} root - The root directory path for this session.
 * @property {string} assets - The directory path for assets within this session.
 * @property {string} files - The directory path for user files within this session.
 */

/**
 * @abstract
 * @class Session
 * @description Abstract base class for session management.
 */
class Session {
  /**
   * @param {string} sessionId - The unique identifier for the session.
   * @param {string} rootPath - The root directory path for this session.
   * @param {Assets} assets - The Assets instance for this session.
   * @param {object} options - Additional options for the session.
   */
  constructor(sessionId, rootPath, assets, options = {}) {
    if (new.target === Session) {
      throw new TypeError('Cannot construct Session instances directly');
    }
    this.id = sessionId;
    this.rootPath = rootPath;
    this.options = options;
    this.assets = assets; // Assign the provided Assets instance

    this.paths = {
      root: rootPath,
      assets: path.join(rootPath, 'assets'),
      files: path.join(rootPath, 'files'),
    };
  }

  /**
   * Gets or creates a session.
   * @abstract
   * @param {string} sessionId - The ID of the session.
   * @returns {Promise<Session>} The session object.
   */
  static async getOrCreateSession(sessionId) {
    throw new Error('Not implemented');
  }

  /**
   * Gets an existing session.
   * @abstract
   * @param {string} sessionId - The ID of the session.
   * @returns {Session | undefined} The session object if found, otherwise undefined.
   */
  static getSession(sessionId) {
    throw new Error('Not implemented');
  }

  /**
   * Resolves a relative path to an absolute path within the session context.
   * If the path starts with 'files/', it resolves relative to the filesPath.
   * Otherwise, it resolves relative to the assetsPath.
   * @param {string} relativePath - The relative path to resolve.
   * @returns {string} The absolute path.
   */
  resolvePath(relativePath) {
    if (relativePath.startsWith('files/')) {
      const unprefixedPath = relativePath.substring('files/'.length);
      return this.filePath(unprefixedPath);
    }
    return this.assetsPath(relativePath);
  }

  /**
   * Resolves a relative path to an absolute path within the session's files directory.
   * @param {string} relativePath - The relative path to resolve.
   * @returns {string} The absolute path.
   */
  filePath(relativePath) {
    return path.join(this.paths.files, relativePath);
  }

  /**
   * Resolves a relative path to an absolute path within the session's assets directory.
   * @param {string} relativePath - The relative path to resolve.
   * @returns {string} The absolute path.
   */
  assetsPath(relativePath) {
    return path.join(this.paths.assets, relativePath);
  }

  /**
   * Writes data to a file within the session's files directory.
   * @abstract
   * @param {string} relativePath - The relative path within the files directory.
   * @param {Buffer | string} data - The data to write.
   * @returns {Promise<void>}
   */
  async writeFile(relativePath, data) {
    throw new Error('Not implemented');
  }
}

export { Session };
