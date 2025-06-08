import { makeGroup } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const x = registerOp({
  name: 'x',
  spec: ['shape', ['numbers'], 'shape'],
  code: (id, assets, input, offsets) =>
    makeGroup(offsets.map((offset) => input.move(offset, 0, 0))),
});
