import { Op } from '@jotcad/op';
import { cut as op } from '@jotcad/geometry';

export const cut = Op.registerOp({
  name: 'cut',
  spec: ['shape', ['shapes'], 'shape'],
  code: (assets, input, tools) => op(assets, input, tools),
});
