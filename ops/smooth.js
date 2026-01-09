import './booleanSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './numberSpec.js';
import { smooth as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const smooth = registerOp({
  name: 'smooth',
  spec: [
    'shape',
    [
      'shapes',
      'number',
      'number',
      'number',
      'boolean',
      'boolean',
      'number',
      [
        'options',
        {
          radius: 'number',
          angleThreshold: 'number',
          resolution: 'number',
          skipFairing: 'boolean',
          skipRefine: 'boolean',
          fairingContinuity: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    polylines,
    implicitRadius = 1.0,
    implicitAngleThreshold = 1.0,
    implicitResolution = 4.0,
    implicitSkipFairing = false,
    implicitSkipRefine = false,
    implicitFairingContinuity = 1,
    {
      radius = implicitRadius,
      angleThreshold = implicitAngleThreshold,
      resolution = implicitResolution,
      skipFairing = implicitSkipFairing,
      skipRefine = implicitSkipRefine,
      fairingContinuity = implicitFairingContinuity,
    } = {}
  ) => {
    console.log('ops/smooth.js: polylines=', polylines);
    console.log(
      'ops/smooth.js: radius=',
      radius,
      'angleThreshold=',
      angleThreshold,
      'resolution=',
      resolution,
      'skipFairing=',
      skipFairing,
      'skipRefine=',
      skipRefine,
      'fairingContinuity=',
      fairingContinuity
    );
    return op(
      session.assets,
      input,
      polylines,
      radius,
      angleThreshold,
      resolution,
      skipFairing,
      skipRefine,
      fairingContinuity
    );
  },
});
