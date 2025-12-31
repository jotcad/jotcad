import './shapesSpec.js';

import { hull as geometryHull } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Hull = registerOp({
  name: 'Hull',
  spec: [null, ['shapes'], 'shape'],
  code: (id, context, input, shapes) => {
    return geometryHull(context.assets, shapes);
  },
});

export const hull = registerOp({
  name: 'hull',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, context, input, shapes) =>
    geometryHull(context.assets, [input, ...shapes]),
});
