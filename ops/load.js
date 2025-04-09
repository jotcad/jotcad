import { Op } from '@jsxcad/op';
import { write } from '@jsxcad/sys';

Op.registerOp(
  'Load',
  [null, ['string'], 'shape'],
  (path) => () => read(`shape/${path}`));
