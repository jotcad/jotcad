import { Op } from '@jotcad/op';
import { Box2 as op2 } from '@jotcad/geometry';
import { Box3 as op3 } from '@jotcad/geometry';

export const Box2 = Op.registerOp(
  'Box2',
  [null, ['interval', 'interval'], 'shape'],
  (assets, input, x, y = x) => op2(assets, x, y)
);

export const Box3 = Op.registerOp(
  'Box3',
  [null, ['interval', 'interval', 'interval'], 'shape'],
  (assets, input, x, y = x, z = y) => op3(assets, x, y, z)
);
