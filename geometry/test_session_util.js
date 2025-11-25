import { withAssets } from './assets.js';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { mkdir, rmdir } from 'node:fs/promises';
import os from 'os'; // Import os module for temporary directory
import { FilesystemSession } from '../ops/filesystem_session.js'; // Import the concrete FilesystemSession class
import { Assets } from './assets.js'; // Import Assets class

const TEST_SESSIONS_BASE_DIR = path.join(os.tmpdir(), 'jotcad_test_sessions');

// This function acts as the concrete implementation of getOrCreateSession for tests
const getOrCreateTestSession = async (testName) => {
  const sessionId = createHash('sha256').update(testName).digest('hex');
  const sessionRootPath = path.join(TEST_SESSIONS_BASE_DIR, sessionId);

  // Clear any previous session data for this test
  try {
    await rmdir(sessionRootPath, { recursive: true });
  } catch (e) {
    if (e.code !== 'ENOENT') {
      throw e;
    }
  }

  // Ensure all necessary directories exist
  await mkdir(sessionRootPath, { recursive: true });
  await mkdir(path.join(sessionRootPath, 'assets'), { recursive: true });
  await mkdir(path.join(sessionRootPath, 'files'), { recursive: true });

  const assets = new Assets(path.join(sessionRootPath, 'assets')); // Instantiate Assets
  // Use the concrete FilesystemSession implementation
  const session = new FilesystemSession(sessionId, sessionRootPath, assets); // Pass assets to constructor
  return session;
};

export const withTestAssets = async (testName, op) => {
  const session = await getOrCreateTestSession(testName);
  // The withAssets function is a bit redundant now, but we'll keep it for compatibility.
  // It ensures the asset directory is clean and provides the assets object.
  return await withAssets(session.paths.assets, async (assets) => {
    // The geometry tests expect to receive the assets object directly.
    return await op(assets);
  });
};