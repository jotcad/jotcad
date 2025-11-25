import './shapeSpec.js';
import './stringSpec.js';
import { registerOp } from './op.js';
import { setTag } from '@jotcad/geometry';

export const color = registerOp({
  name: 'color',
  spec: ['shape', ['string'], 'shape'],
  code: (id, session, input, name) => setTag(input, 'color', name),
});
