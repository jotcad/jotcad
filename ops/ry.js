import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const ry = Op.registerOp({
  name: 'ry',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateY(turn)) }),
});
