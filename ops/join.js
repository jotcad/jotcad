import { Op } from '@jotcad/op';
import { join as op } from '@jotcad/geometry';

export const join = Op.registerOp({
  name: 'join',
  spec: ['shape', ['shapes'], 'shape'],
  code: (assets, input, tools) => op(assets, input, tools),
});
