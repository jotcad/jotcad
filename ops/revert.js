import './shapeSpec.js';

import { registerOp } from './op.js';

export const revert = registerOp({
  name: 'revert',
  spec: ['shape', [], 'shape'],
  code: (id, assets, input) => input.revert(),
});
