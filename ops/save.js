import { Op } from '@jotcad/op';
import { write } from '@jotcad/sys';

export const save = Op.registerOp(
  'save',
  ['shape', ['string'], 'shape'],
  (path) => (input) => write(`shape/${path}`, input)
);
