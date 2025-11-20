import { fromJot, toJot } from '@jotcad/geometry';
import { registerOp } from './op.js';
import { readFile, writeFile } from './fs.js';

export const Jot = registerOp({
  name: 'Jot',
  spec: [null, ['string'], 'shape'],
  code: async (opSymbol, assets, path) => {
    const serialization = await readFile(path);
    return fromJot(assets, serialization);
  },
});

export const jot = registerOp({
  name: 'jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (opSymbol, assets, inputShape, path) => {
    const serialization = await toJot(assets, inputShape);
    await writeFile(path, serialization);
    return inputShape;
  },
});
