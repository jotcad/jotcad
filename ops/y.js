import './shapeSpec.js';
import './numbersSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const y = registerOp({
  name: 'y',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    offsets // Changed assets to session
  ) => makeShape({ shapes: offsets.map((offset) => input.move(0, offset, 0)) }),
});
