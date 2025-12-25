import './numberSpec.js';
import './shapeSpec.js';

import { Wrap3 as geometryWrap3 } from '../geometry/wrap.js';
import { registerOp } from './op.js';

export const wrap3 = registerOp({
  name: 'wrap3',
  spec: ['shape', ['number', 'number'], 'shape'],
  code: (id, context, input, alpha, offset) =>
    geometryWrap3(context.assets, input, alpha, offset),
});
