import './numbersSpec.js';

import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const z = Op.registerOp(
  'z',
  ['shape', ['numbers'], 'shape'],
  (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, 0, offset)) })
);
