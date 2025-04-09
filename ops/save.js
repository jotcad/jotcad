import { Op } from '@jsxcad/op';
import { write } from '@jsxcad/sys';

Op.registerOp(
  'save',
  ['shape', ['string'], 'shape'],
  (path) => (input) => write(`shape/${path}`, input));
