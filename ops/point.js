import './vectorSpec.js';

import { Op } from '@jotcad/op';
import { Point as op } from '@jotcad/geometry';

export const Point = Op.registerOp({
  name: 'Point',
  spec: ['shape', ['vector3'], 'shape'],
  code: (assets, input, vector = []) => op(assets, ...vector),
});
