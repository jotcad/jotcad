import './shapeSpec.js';
import './shapesSpec.js';
import { join as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const join = registerOp({
  name: 'join',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, tools) => op(session.assets, input, tools),
});
