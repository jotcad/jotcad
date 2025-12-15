import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import { join as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const join = registerOp({
  name: 'join',
  spec: [
    'shape',
    ['shapes', ['options', { selfTouchEnvelopeSize: 'number' }]],
    'shape',
  ],
  code: (id, session, input, tools, { selfTouchEnvelopeSize = 0.01 } = {}) =>
    op(session.assets, input, tools, selfTouchEnvelopeSize),
});
