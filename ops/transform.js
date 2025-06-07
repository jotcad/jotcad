import './shapeSpec.js';

import { Op } from '@jotcad/op';

export const transform = Op.registerOp({
  name: 'transform',
  spec: ['shape', ['shape'], 'shape'],
  code: (assets, input, tf) => input.transform(tf.tf),
});
