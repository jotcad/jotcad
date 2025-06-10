import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const y = registerOp({
  name: 'y',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, offset, 0)) }),
});
