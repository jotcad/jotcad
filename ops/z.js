import './numbersSpec.js';

import { Point, makeShape } from '@jotcad/geometry';

import { registerOp } from './op.js';

export const z = registerOp({
  name: 'z',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    offsets // Changed assets to session
  ) =>
    makeShape({
      shapes: offsets.map((offset) => input.move(0, 0, offset)),
    }),
});

export const Z = registerOp({
  name: 'Z',
  spec: [null, ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    offsets // Changed assets to session
  ) =>
    makeShape({
      shapes: offsets.map(
        (offset) => Point(session.assets, 0, 0, 0).move(0, 0, offset) // Use session.assets
      ),
    }),
});
