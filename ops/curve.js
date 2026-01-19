import './shapeSpec.js';
import './shapesSpec.js';
import './numberSpec.js';
import './booleanSpec.js';
import './optionsSpec.js';
import { Curve as geometryCurve } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Curve = registerOp({
  name: 'Curve',
  spec: [
    null,
    ['shapes', ['options', { closed: 'boolean', resolution: 'number' }]],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    shapes,
    { closed = false, resolution = 10 } = {}
  ) => geometryCurve(session.assets, shapes, { closed, resolution }),
});

export const curve = registerOp({
  name: 'curve',
  spec: [
    'shape',
    ['shapes', ['options', { closed: 'boolean', resolution: 'number' }]],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    shapes,
    { closed = false, resolution = 10 } = {}
  ) =>
    geometryCurve(session.assets, [input, ...shapes], { closed, resolution }),
});
