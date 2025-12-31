import './shapeSpec.js';
import './shapesSpec.js';

import { clipOpen as geometryClipOpen } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const clipOpen = registerOp({
  name: 'clipOpen',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, context, from_shape, to_shapes) =>
    geometryClipOpen(context.assets, from_shape, to_shapes),
});
