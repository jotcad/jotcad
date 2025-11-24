import './shapeSpec.js';
import { makeAbsolute } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const absolute = registerOp({
  name: 'absolute',
  spec: ['shape', [], 'shape'],
  code: (id, session, input) => makeAbsolute(session.assets, input),
});
