import { registerOp } from './op.js';

export const mask = registerOp({
  name: 'mask',
  spec: ['shape', ['shape'], 'shape'],
  code: (id, assets, input, mask) => input.derive({ mask }),
});
