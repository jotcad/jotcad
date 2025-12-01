import { access, mkdir, readFile, rmdir, writeFile } from 'node:fs/promises';

import { Assets } from '../geometry/assets.js';
import { FilesystemSession } from './filesystem_session.js';
import { createHash } from 'node:crypto';
import { testJot as geometryTestJot } from '../geometry/test_jot.js';
import { testPng as geometryTestPng } from '../geometry/test_png.js';
import os from 'os';
import path from 'node:path';

const TEST_SESSIONS_BASE_DIR = path.join(os.tmpdir(), 'jotcad_test_sessions');

const getOrCreateTestSession = async (testName) => {
  const sessionId = createHash('sha256').update(testName).digest('hex');
  const sessionRootPath = path.join(TEST_SESSIONS_BASE_DIR, sessionId);

  try {
    await rmdir(sessionRootPath, { recursive: true });
  } catch (e) {
    if (e.code !== 'ENOENT') {
      throw e;
    }
  }

  await mkdir(sessionRootPath, { recursive: true });
  await mkdir(path.join(sessionRootPath, 'assets'), { recursive: true });
  await mkdir(path.join(sessionRootPath, 'files'), { recursive: true });

  const assets = new Assets(path.join(sessionRootPath, 'assets'));
  const session = new FilesystemSession(sessionId, sessionRootPath, assets);
  return session;
};

export const withTestSession = async (testName, op) => {
  const session = await getOrCreateTestSession(testName);
  return await op(session);
};

export const testPng = async (
  session,
  expectedPngFilename, // This is relative to the ops/ directory
  observedPngSessionFilename, // This is the filename of the observed PNG in the session's assets
  threshold = 1000
) => {
  // Construct the absolute path to the expected PNG file, as it lives in the source directory.
  const absoluteExpectedPngPath = `${
    import.meta.dirname
  }/${expectedPngFilename}`;

  // Read the observed PNG buffer from the session's files directory (CRITICAL FIX).
  const observedPngBuffer = await readFile(
    session.filePath(observedPngSessionFilename)
  );

  // Explicitly copy the observed PNG to the source directory for manual review.
  const manualReviewPathInSource = `${
    import.meta.dirname
  }/observed.${expectedPngFilename}`;
  await writeFile(manualReviewPathInSource, observedPngBuffer);

  const result = await geometryTestPng(
    absoluteExpectedPngPath,
    observedPngBuffer,
    threshold
  );

  return result;
};

export const testJot = async (
  session,
  expectedJotFilename, // This is relative to the ops/ directory
  observedJotSessionFilename, // This is the filename of the observed JOT in the session's assets
  threshold = 0 // For text comparison, usually 0
) => {
  // Construct the absolute path to the expected JOT file, as it lives in the source directory.
  const absoluteExpectedJotPath = `${
    import.meta.dirname
  }/${expectedJotFilename}`;

  // Read the observed JOT buffer from the session's files directory.
  const observedJotBuffer = await readFile(
    session.filePath(observedJotSessionFilename)
  );

  // Explicitly copy the observed JOT to the source directory for manual review.
  const manualReviewPathInSource = `${
    import.meta.dirname
  }/observed.${expectedJotFilename}`;
  await writeFile(manualReviewPathInSource, observedJotBuffer);

  const result = await geometryTestJot(
    absoluteExpectedJotPath,
    observedJotBuffer,
    threshold
  );

  return result;
};
