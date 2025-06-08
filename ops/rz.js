import { makeShape } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const rz = registerOp({
  name: 'rz',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateZ(turn)) }),
});
