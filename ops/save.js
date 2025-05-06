import { Op } from '@jotcad/op';
import { write } from '@jotcad/sys';

export const save = Op.registerOp(
  'save',
  ['shape', ['string'], 'shape'],
  (assets, input, path) => write(`shape/${path}`, input)
);
