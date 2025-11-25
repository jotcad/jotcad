import { fromJot, toJot } from '@jotcad/geometry';
import { readFile } from './fs.js';

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
    const serialization = await toJot(
      session.assets,
      inputShape,
      `files/${sessionRelativePath}`
    );
    await session.writeFile(sessionRelativePath, serialization); // MODIFIED
    return inputShape;
  },
});
