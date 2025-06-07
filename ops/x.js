import { Op } from '@jotcad/op';
import { makeGroup } from '@jotcad/geometry';

export const x = Op.registerOp({
  name: 'x',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeGroup(offsets.map((offset) => input.move(offset, 0, 0))),
});
