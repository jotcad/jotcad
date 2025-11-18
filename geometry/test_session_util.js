import { withAssets } from './assets.js';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { mkdir } from 'node:fs/promises';
import os from 'os'; // Import os module for temporary directory

const TEST_SESSIONS_BASE_DIR = path.join(os.tmpdir(), 'jotcad_test_sessions');

const getOrCreateTestSession = async (testName) => {
  const sessionId = createHash('sha256').update(testName).digest('hex');
  const sessionPath = path.join(TEST_SESSIONS_BASE_DIR, sessionId);
  const assetsPath = path.join(sessionPath, 'assets');
  // Ensure the assets directory exists
  await mkdir(assetsPath, { recursive: true });
  return {
    id: sessionId,
    path: sessionPath,
    assetsPath: assetsPath,
  };
};

export const withTestAssets = async (testName, op) => {
  const session = await getOrCreateTestSession(testName);
  return await withAssets(session.assetsPath, op);
};