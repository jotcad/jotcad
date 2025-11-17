import './numbersSpec.js';

import { Point, makeShape } from '@jotcad/geometry';

import { registerOp } from './op.js';

export const z = registerOp({
  name: 'z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, offsets) => {
    if (input === undefined) {
      throw new Error(
        'The "z" op requires an input shape. Did you mean to use "Z"?'
      );
    }
    return makeShape({
      shapes: offsets.map((offset) => input.move(0, 0, offset)),
    });
  },
});

export const Z = registerOp({
  name: 'Z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, offsets) =>
    makeShape({
      shapes: offsets.map((offset) =>
        Point(assets, 0, 0, 0).move(0, 0, offset)
      ),
    }),
});
