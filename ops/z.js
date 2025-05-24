import { Op } from '@jotcad/op';

export const z = Op.registerOp(
  'z',
  ['shape', ['number'], 'shape'],
  (assets, input, offset) => input.move(0, 0, offset)
);
