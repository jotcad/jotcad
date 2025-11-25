import { access, mkdir, readFile, rmdir, writeFile } from 'node:fs/promises';

import { Assets } from '../geometry/assets.js';
import { FilesystemSession } from './filesystem_session.js';
import { createHash } from 'node:crypto';
import os from 'os';
import path from 'node:path';
import pixelmatch from 'pixelmatch';
import pngjs from 'pngjs';

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

const isFilePresent = async (filePath) => {
  try {
    await access(filePath);
    return true;
  } catch (error) {
    return false;
  }
};

export const testPng = async (
  session,
  expectedPngFilename,
  observedPng,
  threshold = 1000
) => {
  const expectedPngPath = session.assets.pathTo(expectedPngFilename);
  const observedPngPath = session.filePath(`observed.${expectedPngFilename}`);

  if (observedPng) {
    await writeFile(observedPngPath, Buffer.from(observedPng));
  }
  const observedPngImage = pngjs.PNG.sync.read(await readFile(observedPngPath));
  const { width, height } = observedPngImage;
  let numFailedPixels = 0;
  if (await isFilePresent(expectedPngPath)) {
    const expectedPngImage = pngjs.PNG.sync.read(
      await readFile(expectedPngPath)
    );
    const differencePng = new pngjs.PNG({ width, height });
    numFailedPixels = pixelmatch(
      expectedPngImage.data,
      observedPngImage.data,
      differencePng.data,
      width,
      height,
      {
        threshold: 0.01,
        alpha: 0.2,
        diffMask: process.env.FORCE_COLOR === '0' ? false : true,
        diffColor:
          process.env.FORCE_COLOR === '0' ? [255, 255, 255] : [255, 0, 0],
      }
    );
  }
  if (numFailedPixels < threshold) {
    if (observedPng) {
      await writeFile(expectedPngPath, Buffer.from(observedPng));
    }
    return true;
  } else {
    console.log(
      `testPng: ${expectedPngPath} differs from ${observedPngPath} by ${numFailedPixels} pixels`
    );
    return false;
  }
};
