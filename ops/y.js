import { Op } from '@jotcad/op';

export const y = Op.registerOp(
  'y',
  ['shape', ['number'], 'shape'],
  (assets, input, offset) => input.move(0, offset, 0)
);
