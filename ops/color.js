import { Op } from '@jotcad/op';
import { setTag } from '@jotcad/geometry';

export const color = Op.registerOp({
  name: 'color',
  spec: ['shape', ['string'], 'shape'],
  code: (assets, input, name) => setTag(input, 'color', name),
});
