import { Op } from '@jotcad/op';
import { write } from '@jotcad/sys';
import { tag } from '@jotcad/geometry';

export const color = Op.registerOp(
  'color',
  ['shape', ['string'], 'shape'],
  (name) => (shape) => tag(shape, 'color', name)
);
