import { Op } from '@jotcad/op';
import { tag } from '@jotcad/geometry';

export const color = Op.registerOp({
  name: 'color',
  spec: ['shape', ['string'], 'shape'],
  code: (assets, input, name) => tag(input, 'color', name),
});
