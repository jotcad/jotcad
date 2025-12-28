import './shapesSpec.js';
import './shapeSpec.js';
import { cutOpen as cutOpenGeom } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const cutOpen = registerOp({
  name: 'cutOpen',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, tools) =>
    cutOpenGeom(session.assets, input, tools),
});
