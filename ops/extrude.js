import './shapeSpec.js';
import { makeShape } from '@jotcad/geometry';
import { nth } from './nth.js';
import { extrude2 as op2 } from '@jotcad/geometry';
import { extrude3 as op3 } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const extrude2 = registerOp({
  name: 'extrude2',
  spec: ['shape', ['shape', 'shape'], 'shape'],
  args: (input, top, bottom) => [top?.nth(0), bottom?.nth(0)],
  code: (id, assets, input, top = makeShape(), bottom = makeShape()) =>
    op2(assets, input, top, bottom),
});

export const extrude3 = registerOp({
  name: 'extrude3',
  spec: ['shape', ['shape', 'shape'], 'shape'],
  args: (input, top, bottom) => [top?.nth(0), bottom?.nth(0)],
  code: (id, assets, input, top = makeShape(), bottom = makeShape()) => {
    console.log(`QQ/extrude3: input=${input}`);
    return op3(assets, input, top, bottom);
  },
});
