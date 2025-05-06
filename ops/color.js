import { Op } from '@jotcad/op';
import { tag } from '@jotcad/geometry';
import { write } from '@jotcad/sys';

export const color = Op.registerOp(
  'color',
  ['shape', ['string'], 'shape'],
  (assets, input, name) => tag(input, 'color', name)
);
