import { Op } from '@jotcad/op';
import { write } from '@jotcad/sys';

export const Load = Op.registerOp(
  'Load',
  [null, ['string'], 'shape'],
  (path) => () => read(`shape/${path}`)
);
