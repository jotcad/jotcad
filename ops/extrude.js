import { Op } from '@jotcad/op';
import { nth } from './nth.js';
import { extrude as op } from '@jotcad/geometry';

export const extrude = Op.registerOp({
  name: 'extrude',
  spec: ['shape', ['shape', 'shape'], 'shape'],
  args: (top, bottom) => [top?.nth(0), bottom?.nth(0)],
  code: (assets, input, top = input, bottom = input) =>
    op(assets, input, top, bottom),
});
