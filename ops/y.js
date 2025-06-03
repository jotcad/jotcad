import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const y = Op.registerOp({
  name: 'y',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, offset, 0)) }),
});
