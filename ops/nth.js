import './shapeSpec.js';
import './numbersSpec.js';
import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const nth = registerOp({
  name: 'nth',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, session, input, indices) => {
    // Changed assets to session
    const shapes = input.nth(...indices);
    if (shapes.length == 1) {
      return shapes[0];
    } else {
      return makeShape({ shapes });
    }
  },
});
