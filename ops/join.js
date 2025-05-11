import { Op } from '@jotcad/op';
import { join as op } from '@jotcad/geometry';

export const join = Op.registerOp(
  'join',
  ['shape', ['shapes'], 'shape'],
  (assets, input, tools) => op(assets, input, tools)
);
