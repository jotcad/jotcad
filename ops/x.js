import { Op } from '@jotcad/op';

export const x = Op.registerOp(
  'x',
  ['shape', ['number'], 'shape'],
  (assets, input, offset) => input.move(offset, 0, 0)
);
