import { Op } from '@jsxcad/op';
import { color as op } from '@jsxcad/geometry';

export const color = Op.registerOp(
  'color',
  ['inputGeometry', 'string'],
  (name) => (input) => op(input, name));
