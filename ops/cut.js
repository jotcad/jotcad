import { Op } from '@jotcad/op';
import { cut as op } from '@jotcad/geometry';

export const cut = Op.registerOp(
  'cut',
  ['shape', ['shapes'], 'shape'],
  (assets, input, tools) => op(assets, input, tools)
);
