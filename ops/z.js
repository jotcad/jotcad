import './numbersSpec.js';

import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const z = Op.registerOp({
  name: 'z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, 0, offset)) }),
});
