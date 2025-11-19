import { fromJot, toJot } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Jot = registerOp({
  name: 'Jot',
  spec: [['string'], 'shape'],
  code: async (id, assets) => fromJot(assets, id),
});

export const jot = registerOp({
  name: 'jot',
  spec: ['shape', ['string'], 'shape'],
  code: async (input, id, assets) => {
    await toJot(assets, input);
    return input;
  },
});
