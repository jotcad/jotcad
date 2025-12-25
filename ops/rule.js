import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';

import { rule as geometryRule } from '../geometry/rule.js';
import { registerOp } from './op.js';

export const Rule = registerOp({
  name: 'Rule',
  spec: [null, ['shapes', ['options', {}]], 'shape'],
  code: (id, context, input, shapes, options = {}) =>
    geometryRule(context.assets, shapes, options),
});

export const rule = registerOp({
  name: 'rule',
  spec: ['shape', ['shapes', ['options', {}]], 'shape'],
  code: (id, context, from_shape, to_shapes, options = {}) =>
    geometryRule(context.assets, [from_shape, ...to_shapes], options),
});
