import './shapeSpec.js';

import { registerOp } from './op.js';

export const transform = registerOp({
  name: 'transform',
  spec: ['shape', ['shape'], 'shape'],
  code: (id, session, input, tf) => input.transform(tf.tf), // Changed assets to session
});
