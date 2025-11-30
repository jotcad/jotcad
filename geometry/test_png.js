import { access, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';

import pixelmatch from 'pixelmatch';
import pngjs from 'pngjs';

const isFilePresent = async (filePath) => {
  try {
    await access(filePath);
    return true;
  } catch (error) {
    return false;
  }
};

export const testPng = async (
  expectedPngFilePath, // Absolute path to the expected PNG
  observedPngBuffer,   // The actual image data as a Buffer
  threshold = 1000
) => {
  const observedPngFilePath = path.join(
    path.dirname(expectedPngFilePath),
    `observed.${path.basename(expectedPngFilePath)}`
  );

  // Always write the observed PNG to the location of the expected PNG for manual review
  if (observedPngBuffer) {
    await writeFile(observedPngFilePath, observedPngBuffer);
  }
  
  const observedPngImage = pngjs.PNG.sync.read(await readFile(observedPngFilePath));
  const { width, height } = observedPngImage;
  let numFailedPixels = 0;
  let expectedFileExists = await isFilePresent(expectedPngFilePath);

  if (!expectedFileExists) {
    console.log(`testPng: Reference image not found at ${expectedPngFilePath}. Creating it.`);
    await writeFile(expectedPngFilePath, observedPngBuffer);
    return true; // The test "passes" by creating the reference
  }

  const expectedPngImage = pngjs.PNG.sync.read(
    await readFile(expectedPngFilePath)
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

  if (numFailedPixels < threshold) {
    // If the test passes and an observedPngBuffer was provided, update the expected reference.
    // This is the original behavior which implicitly "approves" the observed image.
    if (observedPngBuffer) {
      await writeFile(expectedPngFilePath, observedPngBuffer);
    }
    return true;
  } else {
    console.log(
      `testPng: ${expectedPngFilePath} differs from ${observedPngFilePath} by ${numFailedPixels} pixels`
    );
    return false;
  }
};
