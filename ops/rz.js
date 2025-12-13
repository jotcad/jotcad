import './shapeSpec.js';
import './numbersSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const rz = registerOp({
  name: 'rz',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    turns // Changed assets to session
  ) => {
    const rotatedShapes = turns.map((turn) => input.rotateZ(turn));
    return makeShape({ shapes: rotatedShapes });
  },
});
