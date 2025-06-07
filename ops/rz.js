import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const rz = Op.registerOp({
  name: 'rz',
  spec: ['shape', ['numbers'], 'shape'],
  code: (assets, input, turns) =>
    makeShape({ shapes: turns.map((turn) => input.rotateZ(turn)) }),
});
