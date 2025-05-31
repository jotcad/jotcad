import { Op } from '@jotcad/op';
import { clip as op } from '@jotcad/geometry';

export const clip = Op.registerOp({
  name: 'clip',
  spec: ['shape', ['shapes'], 'shape'],
  code: (assets, input, tools) => op(assets, input, tools),
});
