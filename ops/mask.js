import './shapeSpec.js';
import { registerOp } from './op.js';

export const mask = registerOp({
  name: 'mask',
  spec: ['shape', ['shape'], 'shape'],
  code: (id, session, input, mask) => input.derive({ mask }),
});
