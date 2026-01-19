import './shapeSpec.js';
import './numberSpec.js';
import './optionsSpec.js';
import { clean as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const clean = registerOp({
  name: 'clean',
  spec: [
    'shape',
    [
      'number',
      [
        'options',
        {
          angleThreshold: 'number',
          useAngleConstrained: 'boolean',
          regularize: 'boolean',
          collapse: 'boolean',
          planeDistanceThreshold: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    implicitAngleThreshold = 1.0,
    {
      angleThreshold = implicitAngleThreshold,
      useAngleConstrained = true,
      regularize = true,
      collapse = true,
      planeDistanceThreshold = 0.1,
    } = {}
  ) => {
    console.log(
      'ops/clean.js: angleThreshold=',
      angleThreshold,
      'tau',
      'useAngleConstrained=',
      useAngleConstrained,
      'regularize=',
      regularize,
      'collapse=',
      collapse,
      'planeDistanceThreshold=',
      planeDistanceThreshold
    );
    return op(
      session.assets,
      input,
      angleThreshold,
      useAngleConstrained,
      regularize,
      collapse,
      planeDistanceThreshold
    );
  },
});
