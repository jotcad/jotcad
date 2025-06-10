import './vectorSpec.js';

import { Point as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Point = registerOp({
  name: 'Point',
  spec: ['shape', ['vector3'], 'shape'],
  code: (id, assets, input, vector = []) => op(assets, ...vector),
});
