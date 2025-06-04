import { Op } from '@jotcad/op';
import { makeAbsolute } from '@jotcad/geometry';

export const absolute = Op.registerOp({
  name: 'absolute',
  spec: ['shape', [], 'shape'],
  code: (assets, input) => makeAbsolute(assets, input),
});
