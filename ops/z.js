import './numbersSpec.js';

import { Point, makeShape } from '@jotcad/geometry';

import { Op } from '@jotcad/op';

export const z = Op.registerOp({
  name: 'z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeShape({ shapes: offsets.map((offset) => input.move(0, 0, offset)) }),
});

export const Z = Op.registerOp({
  name: 'Z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, offsets) =>
    makeShape({
      shapes: offsets.map((offset) =>
        Point(assets, 0, 0, 0).move(0, 0, offset)
      ),
    }),
});
