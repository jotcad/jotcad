import './numberSpec.js';
import './shapeSpec.js';

import { Point as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Point = registerOp({
  name: 'Point',
  spec: ['shape', ['number', 'number', 'number'], 'shape'],
  code: (id, session, input, x = 0, y = 0, z = 0) =>
    op(session.assets, 0, 0, 0).move(x, y, z),
});
