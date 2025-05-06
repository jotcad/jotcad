import { Op } from '@jotcad/op';
import { extrude as op } from '@jotcad/geometry';

export const extrude = Op.registerOp(
  'extrude',
  ['shape', ['shape', 'shape'], 'shape'],
  (assets, input, top, bottom) => op(assets, input, top, bottom)
);
