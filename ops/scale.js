import './shapeSpec.js';
import './numbersSpec.js';
import { makeGroup } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const scale = registerOp({
  name: 'scale',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, session, input, [x = 1, y = 1, z = 1]) => input.scale(x, y, z),
});
