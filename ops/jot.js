import { fromJot, toJot } from '@jotcad/geometry';
import { readFile, writeFile } from './fs.js';
import path from 'node:path'; // Import the path module

import { registerOp } from './op.js';

export const Jot = registerOp({
  name: 'Jot',
  spec: [null, ['string'], 'shape'],
  code: async (opSymbol, session, externalFilePath) => {
    // Changed assets to session
    const serialization = await readFile(externalFilePath);
    return fromJot(session.assets, serialization); // Use session.assets
  },
});

export const jot = registerOp({
  name: 'jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (opSymbol, session, inputShape, sessionRelativePath) => {
    // Changed assets to session
    // Renamed 'path' to 'sessionRelativePath'
    const serialization = await toJot(
      session.assets, // Use session.assets
      inputShape,
      `files/${sessionRelativePath}`
    );
    await writeFile(session.filePath(sessionRelativePath), serialization); // Use session.filePath
    return inputShape;
  },
});
