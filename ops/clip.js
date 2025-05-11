import { Op } from '@jotcad/op';
import { clip as op } from '@jotcad/geometry';

export const clip = Op.registerOp(
  'clip',
  ['shape', ['shapes'], 'shape'],
  (assets, input, tools) => op(assets, input, tools)
);
