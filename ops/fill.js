import { fill2 as op2 } from '@jotcad/geometry';
import { fill3 as op3 } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const fill2 = registerOp({
  name: 'fill2',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, shapes) => op2(assets, [input, ...shapes], true),
});

export const fill3 = registerOp({
  name: 'fill3',
  spec: ['shape', [], 'shape'],
  code: (id, assets, input) => op3(assets, input),
});
