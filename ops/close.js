import './shapeSpec.js';
import { close3 as close3Geom } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const close3 = registerOp({
  name: 'close3',
  spec: ['shape', [], 'shape'],
  code: (id, session, input) => close3Geom(session.assets, input),
});
