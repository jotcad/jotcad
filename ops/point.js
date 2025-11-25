import './vectorSpec.js';
import './shapeSpec.js';

import { Point as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Point = registerOp({
  name: 'Point',
  spec: ['shape', ['vector3'], 'shape'],
  code: (id, session, input, vector = []) => op(session.assets, ...vector),
});
