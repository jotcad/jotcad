import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const x = Op.registerOp(
  'x',
  ['shape', ['numbers'], 'shape'],
  (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(offset, 0, 0)) })
);
