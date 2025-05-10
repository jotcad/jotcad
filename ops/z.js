import { Op } from '@jotcad/op';
import { extrude as op } from '@jotcad/geometry';

export const z = Op.registerOp(
  'z',
  ['shape', ['number'], 'shape'],
  (assets, input, offset) => input.move(0, 0, offset)
);
