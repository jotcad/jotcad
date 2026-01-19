import './shapeSpec.js';
import './numberSpec.js';
import './optionsSpec.js';
import { edges as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const edges = registerOp({
  name: 'edges',
  spec: [
    'shape',
    ['number', ['options', { angleThreshold: 'number' }]],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    implicitAngleThreshold = 0.1,
    { angleThreshold = implicitAngleThreshold } = {}
  ) => {
    return op(session.assets, input, angleThreshold);
  },
});
