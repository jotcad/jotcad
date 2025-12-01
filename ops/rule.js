import './optionsSpec.js';
import './shapeSpec.js';

import { rule as geometryRule } from '../geometry/rule.js';
import { registerOp } from './op.js';

export const Rule = registerOp({
  name: 'Rule',
  spec: [null, ['shape', 'shape', ['options', {}]], 'shape'],
  code: (id, context, input, from_shape, to_shape, options = {}) =>
    geometryRule(context.assets, from_shape, to_shape, options),
});

export const rule = registerOp({
  name: 'rule',
  spec: ['shape', ['shape', ['options', {}]], 'shape'],
  code: (id, context, from_shape, to_shape, options = {}) =>
    geometryRule(context.assets, from_shape, to_shape, options),
});
