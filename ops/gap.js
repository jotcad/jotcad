import './shapeSpec.js';
import { registerOp } from './op.js';
import { setTag } from '@jotcad/geometry';

export const gap = registerOp({
  name: 'gap',
  spec: ['shape', [], 'shape'],
  code: (id, session, input) => setTag(input, 'isGap', true),
});
