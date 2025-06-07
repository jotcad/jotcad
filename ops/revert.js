import './shapeSpec.js';

import { Op } from '@jotcad/op';

export const revert = Op.registerOp({
  name: 'revert',
  spec: ['shape', [], 'shape'],
  code: (assets, input) => input.revert(),
});
