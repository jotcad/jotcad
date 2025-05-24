import { Op } from '@jotcad/op';
import { simplify as op } from '@jotcad/geometry';

export const simplify = Op.registerOp(
  'simplify',
  [null, ['number', ['options', { faceCount: 'number' }]], 'shape'],
  (
    assets,
    input,
    implicitFaceCount = 10000,
    { faceCount = implicitFaceCount } = {}
  ) => op(assets, input, { faceCount })
);
