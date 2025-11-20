import { fromJot, toJot } from '@jotcad/geometry';
import { registerOp } from './op.js';
import { writeFile } from './fs.js';

export const Jot = registerOp({
  name: 'Jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (opSymbol, assets, id) => fromJot(assets, id),
});

export const jot = registerOp({
  name: 'jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (opSymbol, assets, inputShape, path) => {
    const jotId = await toJot(assets, inputShape);
    await writeFile(path, jotId);
    return inputShape;
  },
});
