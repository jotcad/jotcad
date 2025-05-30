import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const y = Op.registerOp(
  'y',
  ['shape', ['numbers'], 'shape'],
  (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, offset, 0)) })
);
