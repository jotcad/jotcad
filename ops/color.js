import { Op } from '@jotcad/op';
import { tag } from '@jotcad/geometry';

export const color = Op.registerOp(
  'color',
  ['shape', ['string'], 'shape'],
  (assets, input, name) => tag(input, 'color', name)
);
