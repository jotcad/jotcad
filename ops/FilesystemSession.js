import {
  mkdir,
  readdir,
  rename,
  rm,
  rmdir,
  stat,
  unlink,
} from 'node:fs/promises';
import { Session } from './Session.js';
import { createHash } from 'node:crypto';
import path from 'node:path';

const SESSIONS_DIR = '/tmp/jotcad/sessions';
const ONE_HOUR_IN_MS = 60 * 60 * 1000;

const activeSessions = new Map(); // Keep map for active sessions by ID

/**
 * Concrete implementation of the Session abstraction that operates directly on the filesystem.
 */
class FilesystemSession extends Session {
  constructor(sessionId, rootPath, assets) {
    // Add assets argument
    super(sessionId, rootPath, assets); // Pass assets to super
    this.createdAt = Date.now(); // Fixed typo
  }

  static async getOrCreateSession(sessionId) {
    if (activeSessions.has(sessionId)) {
      return activeSessions.get(sessionId);
    }

    const sessionRootPath = path.join(
      SESSIONS_DIR,
      createHash('sha256').update(sessionId).digest('hex')
    );

    // Ensure all necessary directories exist
    await mkdir(sessionRootPath, { recursive: true });
    await mkdir(path.join(sessionRootPath, 'assets'), { recursive: true });
    await mkdir(path.join(sessionRootPath, 'files'), { recursive: true });

    // The Assets instance will be created by the calling code (server/session.js)
    // and passed to the FilesystemSession constructor.
    const assets = null; // Placeholder: assets will be created by caller.
    const session = new FilesystemSession(sessionId, sessionRootPath, assets); // Pass assets to constructor
    activeSessions.set(sessionId, session);

    return session;
  }

  static getSession(sessionId) {
    return activeSessions.get(sessionId);
  }

  // --- Cleanup logic adapted from server/session.js ---

  // New function to check if a directory on disk is expired
  static isDirectoryExpired(dirPath, mtime) {
    const now = Date.now();
    return dirPath.endsWith('.deleteme') || now - mtime > ONE_HOUR_IN_MS;
  }

  // Helper function to delete the known contents of a session directory (non-recursive)
  static async deleteSessionDirectoryContents(sessionPath, sessionId) {
    const assetsPath = path.join(sessionPath, 'assets');
    const filesPath = path.join(sessionPath, 'files');
    const resultPath = path.join(sessionPath, 'result');
    const textPath = path.join(sessionPath, 'text');

    // Function to delete files within a directory and then the directory itself (non-recursive)
    const deleteFilesAndDirectory = async (dirPath, type) => {
      try {
        // Use rm with recursive: true to ensure the directory and its contents are fully removed
        await rm(dirPath, { recursive: true, force: true });
        return true; // Indicate success
      } catch (err) {
        if (err.code === 'ENOENT') {
          return true; // Not found is also a form of success for cleanup
        } else {
          return false; // Indicate failure
        }
      }
    };

    const assetsCleaned = await deleteFilesAndDirectory(assetsPath, 'assets');
    const filesCleaned = await deleteFilesAndDirectory(filesPath, 'files');
    // text and result are subdirectories of assets, so they are cleaned when assetsPath is cleaned.
    // const resultCleaned = await deleteFilesAndDirectory(resultPath, 'result');
    // const textCleaned = await deleteFilesAndDirectory(textPath, 'text');

    // Only attempt to delete the session directory itself if all expected subdirectories were cleaned
    if (assetsCleaned && filesCleaned) {
      // && resultCleaned && textCleaned) {
      try {
        await rmdir(sessionPath);
      } catch (err) {
        if (err.code === 'ENOENT') {
        } else if (err.code === 'ENOTEMPTY') {
        } else {
        }
      }
    } else {
    }
  }

  static async cleanupSessions() {
    try {
      // Ensure SESSIONS_DIR exists before trying to read it
      await mkdir(SESSIONS_DIR, { recursive: true });

      const sessionDirs = await readdir(SESSIONS_DIR);
      for (const dirName of sessionDirs) {
        const sessionPath = path.join(SESSIONS_DIR, dirName);
        let sessionId = dirName; // Use dirName as sessionId for logging purposes

        // Try to get stats to check modification time
        let stats;
        try {
          stats = await stat(sessionPath);
        } catch (err) {
          if (err.code === 'ENOENT') {
            continue;
          }
          continue;
        }

        if (FilesystemSession.isDirectoryExpired(sessionPath, stats.mtimeMs)) {
          await FilesystemSession.deleteSessionDirectoryContents(
            sessionPath,
            sessionId
          );
          // Remove from activeSessions map if it was there
          // This requires mapping dirName (hash) back to original sessionId, which is not straightforward.
          // For now, rely on filesystem cleanup.
        } else {
        }
      }
    } catch (error) {
      if (error.code === 'ENOENT') {
      } else {
      }
    }
  }

  static startCleanup() {
    // Run cleanup immediately and then every hour
    FilesystemSession.cleanupSessions();
    setInterval(FilesystemSession.cleanupSessions, ONE_HOUR_IN_MS);
  }
}

export { FilesystemSession };
