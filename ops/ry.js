import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const ry = registerOp({
  name: 'ry',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateY(turn)) }),
});
