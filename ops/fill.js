import { Op } from '@jotcad/op';
import { fill2 as op2 } from '@jotcad/geometry';
import { fill3 as op3 } from '@jotcad/geometry';

export const fill2 = Op.registerOp(
  'fill2',
  ['shape', ['shapes'], 'shape'],
  (assets, input, shapes) => op2(assets, [input, ...shapes], true)
);

export const fill3 = Op.registerOp(
  'fill3',
  ['shape', [], 'shape'],
  (assets, input) => op3(assets, input)
);
