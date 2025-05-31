import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const nth = Op.registerOp({
  name: 'nth',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, indices) => {
    const shapes = input.nth(...indices);
    if (shapes.length == 1) {
      return shapes[0];
    } else {
      return makeShape({ shapes });
    }
  },
});
