import { Op } from '@jotcad/op';
import { fill as op } from '@jotcad/geometry';

export const fill = Op.registerOp(
  'fill',
  ['shape', [], 'shape'],
  (assets, input) => op(assets, [input], true)
);
