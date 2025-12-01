import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';

import { Link as geometryLink } from '../geometry/link.js';
import { registerOp } from './op.js';

export const Link = registerOp({
  name: 'Link',
  spec: [null, ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, shapes, false, reverse),
});

export const link = registerOp({
  name: 'link',
  spec: ['shape', ['shapes'], ['options', { reverse: 'boolean' }], 'shape'],
  code: (id, context, input, shapes, { reverse = false } = {}) =>
    geometryLink(context.assets, [input, ...shapes], false, reverse),
});
