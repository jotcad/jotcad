import './shapeSpec.js';
import './stringSpec.js';
import { registerOp } from './op.js';
import { setTag } from '@jotcad/geometry';

export const material = registerOp({
  name: 'material',
  spec: ['shape', ['string'], 'shape'],
  code: (id, session, input, name) => setTag(input, 'material', name),
});
