import { mkdir, readdir, rename, rm, rmdir, stat, unlink } from 'node:fs/promises'; // Added stat
import { createHash } from 'node:crypto';
import path from 'node:path';

const SESSIONS_DIR = '/tmp/jotcad/sessions';
const ONE_HOUR_IN_MS = 60 * 60 * 1000;

const sessions = new Map(); // Keep map for active sessions

export const getOrCreateSession = async (sessionId) => {
  if (sessions.has(sessionId)) {
    return sessions.get(sessionId);
  }
  const sessionPath = path.join(SESSIONS_DIR, createHash('sha256').update(sessionId).digest('hex'));
  const assetsPath = path.join(sessionPath, 'assets');
  const filesPath = path.join(sessionPath, 'files');
  const session = {
    id: sessionId,
    path: sessionPath,
    assetsPath: assetsPath,
    filesPath: filesPath,
    createdAt: Date.now(), // Store creation time
  };
  sessions.set(sessionId, session);
  await mkdir(assetsPath, { recursive: true });
  await mkdir(filesPath, { recursive: true });
  return session;
};

export const getSession = (sessionId) => sessions.get(sessionId);

// New function to check if a directory on disk is expired
const isDirectoryExpired = (dirPath, mtime) => {
  const now = Date.now();
  return dirPath.endsWith('.deleteme') || (now - mtime) > ONE_HOUR_IN_MS;
};

// Helper function to delete the known contents of a session directory (non-recursive)
const deleteSessionDirectoryContents = async (sessionPath, sessionId) => {
  console.log(`Attempting to clean up directory: ${sessionPath} for session ${sessionId}`);

  const assetsPath = path.join(sessionPath, 'assets');
  const filesPath = path.join(sessionPath, 'files');
  const resultPath = path.join(sessionPath, 'result');
  const textPath = path.join(sessionPath, 'text');

  // Function to delete files within a directory and then the directory itself (non-recursive)
  const deleteFilesAndDirectory = async (dirPath, type) => {
    try {
      // Use rm with recursive: true to ensure the directory and its contents are fully removed
      await rm(dirPath, { recursive: true, force: true });
      console.log(`Deleted ${type} directory and its contents: ${dirPath} in session ${sessionId}`);
      return true; // Indicate success
    } catch (err) {
      if (err.code === 'ENOENT') {
        console.log(`${type} directory not found, skipping: ${dirPath} for session ${sessionId}`);
        return true; // Not found is also a form of success for cleanup
      } else {
        console.error(`Error cleaning ${type} directory ${dirPath} for session ${sessionId}:`, err);
        return false; // Indicate failure
      }
    }
  };

  const assetsCleaned = await deleteFilesAndDirectory(assetsPath, 'assets');
  const filesCleaned = await deleteFilesAndDirectory(filesPath, 'files');
  const resultCleaned = await deleteFilesAndDirectory(resultPath, 'result');
  const textCleaned = await deleteFilesAndDirectory(textPath, 'text');

  // Only attempt to delete the session directory itself if all expected subdirectories were cleaned
  if (assetsCleaned && filesCleaned && resultCleaned && textCleaned) {
    try {
      await rmdir(sessionPath);
      console.log(`Deleted empty session directory: ${sessionPath} for session ${sessionId}`);
    } catch (err) {
      if (err.code === 'ENOENT') {
        console.log(`Session directory not found, skipping: ${sessionPath} for session ${sessionId}`);
      } else if (err.code === 'ENOTEMPTY') {
        console.error(`Error: Session directory ${sessionPath} for session ${sessionId} is not empty (contains unexpected files/subdirectories). Keeping this directory.`, err);
      } else {
        console.error(`Error deleting session directory ${sessionPath} for session ${sessionId}:`, err);
      }
    }
  } else {
    console.log(`Skipping deletion of session directory ${sessionPath} for session ${sessionId} because some expected subdirectories could not be fully cleaned.`);
  }
};


export const cleanupSessions = async () => {
  console.log('Running session cleanup...');

  try {
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
          console.log(`Directory ${sessionPath} not found during scan, skipping.`);
          continue;
        }
        console.error(`Error getting stats for ${sessionPath}:`, err);
        continue;
      }

      console.log(`Considering directory for cleanup: ${sessionPath}`);

      if (isDirectoryExpired(sessionPath, stats.mtimeMs)) {
        console.log(`Decision: Cleaning up directory ${sessionPath}. Rationale: ${sessionPath.endsWith('.deleteme') ? 'Marked .deleteme' : 'Expired by modification time'}`);
        await deleteSessionDirectoryContents(sessionPath, sessionId);
        // If this session was in the map, remove it.
        // We need to derive the original sessionId from dirName if it's a hash.
        // For now, let's assume dirName is the hash.
        // This part needs more thought if we want to remove from the map reliably.
        // For now, we'll just clean up the directory on disk.
      } else {
        console.log(`Decision: Keeping directory ${sessionPath}. Rationale: Not expired.`);
      }
    }
  } catch (error) {
    if (error.code === 'ENOENT') {
      console.log(`Session directory ${SESSIONS_DIR} not found, skipping cleanup scan.`);
    } else {
      console.error(`Error scanning SESSIONS_DIR for cleanup:`, error);
    }
  }
};

export const startCleanup = () => {
  // Run cleanup immediately and then every hour
  cleanupSessions();
  setInterval(cleanupSessions, ONE_HOUR_IN_MS);
  console.log('Session cleanup process started.');
};
