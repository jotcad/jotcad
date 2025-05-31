import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const x = Op.registerOp({
  name: 'x',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(offset, 0, 0)) }),
});
