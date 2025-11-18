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
  const observedPngPath = path.join(assets.basePath, `observed.${expectedPngFilename}`); // Observed PNG goes into assets.basePath
  
  console.log(`[testPng] observedPngPath: ${observedPngPath}`);
  console.log(`[testPng] observedPng is defined: ${observedPng !== undefined && observedPng !== null}`);

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
    } else {
      // TODO: copy the file.
    }
    return true;
  } else {
    console.log(
      `testPng: ${expectedPngPath} differs from ${observedPngPath} by ${numFailedPixels} pixels`
    );
    return false;
  }
};