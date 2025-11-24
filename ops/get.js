import './shapeSpec.js';
import './stringSpec.js';

import { getShapesByTag, makeGroup } from '@jotcad/geometry';

import { registerOp } from './op.js';

export const get = registerOp({
  name: 'get',
  spec: ['shape', ['string', 'string'], 'shape'],
  code: (id, session, input, name, value) => getShapesByTag(input, name, value),
});
