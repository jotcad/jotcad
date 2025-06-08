import { simplify as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const simplify = registerOp({
  name: 'simplify',
  spec: [null, ['number', ['options', { faceCount: 'number' }]], 'shape'],
  code: (
    id,
    assets,
    input,
    implicitFaceCount = 10000,
    { faceCount = implicitFaceCount } = {}
  ) => op(assets, input, { faceCount }),
});
