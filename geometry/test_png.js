import { access, readFile, writeFile } from 'node:fs/promises';
import path, { dirname } from 'node:path'; // Import dirname
import { fileURLToPath } from 'url'; // Import fileURLToPath

import pixelmatch from 'pixelmatch';
import pngjs from 'pngjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const isFilePresent = async (filePath) => {
  try {
    await access(filePath);
    return true;
  } catch (error) {
    return false;
  }
};

export const testPng = async (
  assets, // assets object
  expectedPngFilename, // Now just the filename (e.g., 'arc.test.full.png')
  observedPng,
  threshold = 1000
) => {
  const expectedPngPath = path.join(__dirname, expectedPngFilename); // Construct full path internally
  const observedPngPath = assets.pathTo(`observed.${expectedPngFilename}`); // Observed PNG goes into assets.basePath

  // Always write the observed PNG to the test-specific observed path
  if (observedPng) {
    await writeFile(observedPngPath, Buffer.from(observedPng));
  }
  
  // Also copy to geometry/observed.<png> as requested for manual inspection
  const manualObservedPngPath = path.join(__dirname, `observed.${expectedPngFilename}`);
  if (observedPng) {
    await writeFile(manualObservedPngPath, Buffer.from(observedPng));
  }

  const observedPngImage = pngjs.PNG.sync.read(await readFile(observedPngPath));
  const { width, height } = observedPngImage;
  let numFailedPixels = 0;
  let expectedFileExists = await isFilePresent(expectedPngPath);

  if (!expectedFileExists) {
    console.log(`testPng: Reference image not found at ${expectedPngPath}.`);
    // Fail the test if the reference image is missing.
    return false;
  }

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

  if (numFailedPixels < threshold) {
    // If the test passes and an observedPng was provided, update the expected reference.
    // This is the original behavior which implicitly "approves" the observed image.
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
