import './shapeSpec.js';
import './shapesSpec.js';
import { cut2 as op2, cut3 as op3 } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const cut2 = registerOp({
  name: 'cut2',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, tools) => op2(session.assets, input, tools),
});

export const cut3 = registerOp({
  name: 'cut3',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, tools) => op3(session.assets, input, tools),
});
