import './shapeSpec.js';
import './numbersSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const ry = registerOp({
  name: 'ry',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    turns // Changed assets to session
  ) => makeShape({ shapes: turns.map((turn) => input.rotateY(turn)) }),
});
