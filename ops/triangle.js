import { Op } from '@jsxcad/op';
import { Triangle as op } from '@jsxcad/geometry';

export const Triangle = Op.registerOp(
  'Triangle',
  [null, ['intervals', 'options'], 'shape'],
  op
);
