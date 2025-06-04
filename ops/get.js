import { getShapesByTag, makeGroup } from '@jotcad/geometry';

import { Op } from '@jotcad/op';

export const get = Op.registerOp({
  name: 'get',
  spec: ['shape', ['string', 'string'], 'shape'],
  code: (assets, input, name, value) => getShapesByTag(input, name, value),
});
