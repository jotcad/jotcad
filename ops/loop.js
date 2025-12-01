import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import { Link as geometryLink } from '../geometry/link.js';
import { registerOp } from './op.js';

export const Loop = registerOp({
  name: 'Loop',
  spec: [null, ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, shapes, true, reverse),
});

export const loop = registerOp({
  name: 'loop',
  spec: ['shape', ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, [input, ...shapes], true, reverse),
});
