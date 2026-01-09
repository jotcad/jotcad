import './shapeSpec.js';
import './numbersSpec.js';
import { Point, makeShape, setTag } from '@jotcad/geometry';
import { makeGroup } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const x = registerOp({
  name: 'x',
  spec: ['shape', ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    offsets // Changed assets to session
  ) => makeGroup(offsets.map((offset) => input.move(offset, 0, 0))),
});

export const X = registerOp({
  name: 'X',
  spec: [null, ['numbers'], 'shape'],
  code: (
    id,
    session,
    input,
    offsets // Changed assets to session
  ) =>
    makeShape({
      shapes: offsets.map(
        (offset) =>
          setTag(
            Point(session.assets, 0, 0, 0).move(offset, 0, 0),
            'isPlane',
            true
          ) // Use session.assets
      ),
    }),
});
