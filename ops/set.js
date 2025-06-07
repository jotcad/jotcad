import { Op } from '@jotcad/op';
import { setTag } from '@jotcad/geometry';

export const set = Op.registerOp({
  name: 'set',
  spec: ['shape', ['string', 'string'], 'shape'],
  code: (assets, input, name, value) => setTag(input, name, value),
});
