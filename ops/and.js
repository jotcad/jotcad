import './shapeSpec.js';
import './shapesSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const And = registerOp({
  name: 'And',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, shapes) => makeShape({ shapes }),
});

export const and = registerOp({
  name: 'and',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, shapes) =>
    makeShape({ shapes: [input, ...shapes] }),
});
