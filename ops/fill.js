import './shapeSpec.js';
import './shapesSpec.js';
import { fill2 as op2 } from '@jotcad/geometry';
import { fill3 as op3 } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const fill2 = registerOp({
  name: 'fill2',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, shapes) =>
    op2(session.assets, [input, ...shapes], true),
});

export const fill3 = registerOp({
  name: 'fill3',
  spec: ['shape', [], 'shape'],
  code: (id, session, input) => op3(session.assets, input),
});
