import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const rx = Op.registerOp({
  name: 'rx',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateX(turn)) }),
});
