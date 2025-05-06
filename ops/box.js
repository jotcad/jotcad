import { Op } from '@jotcad/op';
import { Box2 as op } from '@jotcad/geometry';

export const Box2 = Op.registerOp(
  'Box2',
  [null, ['interval', 'interval'], 'shape'],
  (assets, input, c1, c2) => op(assets, c1, c2)
);
