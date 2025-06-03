import { Op } from '@jotcad/op';
import { simplify as op } from '@jotcad/geometry';

export const simplify = Op.registerOp({
  name: 'simplify',
  spec: [null, ['number', ['options', { faceCount: 'number' }]], 'shape'],
  code: (
    assets,
    input,
    implicitFaceCount = 10000,
    { faceCount = implicitFaceCount } = {}
  ) => op(assets, input, { faceCount }),
});
