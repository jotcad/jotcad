import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const rx = registerOp({
  name: 'rx',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateX(turn)) }),
});
