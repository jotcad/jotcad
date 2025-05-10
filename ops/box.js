import { Op } from '@jotcad/op';
import { Box2 as op2 } from '@jotcad/geometry';
import { Box3 as op3 } from '@jotcad/geometry';

export const Box2 = Op.registerOp(
  'Box2',
  [null, ['interval', 'interval'], 'shape'],
  (assets, input, c1, c2) => op2(assets, c1, c2)
);

export const Box3 = Op.registerOp(
  'Box3',
  [null, ['interval', 'interval'], 'shape'],
  (assets, input, c1, c2) => op3(assets, c1, c2)
);
