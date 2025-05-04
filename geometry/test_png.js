import { readFile, writeFile } from 'node:fs/promises';

import pixelmatch from 'pixelmatch';
import pngjs from 'pngjs';

export const testPng = async (expectedPngPath, observedPng, threshold=1000) => {
  const observedPngPath = `observed.${expectedPngPath}`;
  await writeFile(observedPngPath, Buffer.from(observedPng));
  const observedPngImage = pngjs.PNG.sync.read(await readFile(observedPngPath));
  const { width, height } = observedPngImage;
  const expectedPngImage = pngjs.PNG.sync.read(await readFile(expectedPngPath));
  const differencePng = new pngjs.PNG({ width, height });
  const numFailedPixels = pixelmatch(
    expectedPngImage.data,
    observedPngImage.data,
    differencePng.data,
    width,
    height,
    {
      threshold: 0.01,
      alpha: 0.2,
      diffMask: process.env.FORCE_COLOR === '0',
      diffColor:
        process.env.FORCE_COLOR === '0' ? [255, 255, 255] : [255, 0, 0],
    }
  );
  if (numFailedPixels < threshold) {
    await writeFile(expectedPngPath, Buffer.from(observedPng));
    return true;
  } else {
    console.log(`testPng: ${expectedPngPath} differs from ${observedPngPath} by ${numFailedPixels} pixels`);
    return false;
  }
};
