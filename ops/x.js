import './shapeSpec.js';
import './numbersSpec.js';
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
