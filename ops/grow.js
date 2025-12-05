import './shapeSpec.js';
import './shapesSpec.js';
import { grow as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const grow = registerOp({
  name: 'grow',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, tools) => op(session.assets, input, tools),
});
