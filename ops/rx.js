import './shapeSpec.js';
import './numbersSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const rx = registerOp({
  name: 'rx',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    turns // Changed assets to session
  ) => makeShape({ shapes: turns.map((turn) => input.rotateX(turn)) }),
});
