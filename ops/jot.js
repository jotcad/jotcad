import { fromJot, toJot } from '@jotcad/geometry';
import { readFile, writeFile } from './fs.js';
import path from 'node:path'; // Import the path module

import { registerOp } from './op.js';

export const Jot = registerOp({
  name: 'Jot',
  spec: [null, ['string'], 'shape'],
  code: async (opSymbol, assets, externalFilePath) => {
    // Renamed 'path' to 'externalFilePath'
    const serialization = await readFile(externalFilePath);
    return fromJot(assets, serialization);
  },
});

export const jot = registerOp({
  name: 'jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (opSymbol, assets, inputShape, sessionRelativePath) => {
    // Renamed 'path' to 'sessionRelativePath'
    const serialization = await toJot(
      assets,
      inputShape,
      `files/${sessionRelativePath}`
    );
    await writeFile(sessionRelativePath, serialization);
    return inputShape;
  },
});
