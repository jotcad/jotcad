import './shapeSpec.js';

import { registerOp } from './op.js';

export const transform = registerOp({
  name: 'transform',
  spec: ['shape', ['shape'], 'shape'],
  code: (id, assets, input, tf) => input.transform(tf.tf),
});
