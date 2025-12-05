import './shapeSpec.js';
import { footprint as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const footprint = registerOp({
  name: 'footprint',
  spec: ['shape', [], 'shape'],
  code: (id, session, input) => op(session.assets, input),
});
