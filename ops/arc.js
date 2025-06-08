import { Arc2 as op2 } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Arc2 = registerOp({
  name: 'Arc2',
  spec: [
    null,
    [
      'interval',
      'interval',
      [
        'options',
        {
          sides: 'number',
          start: 'number',
          end: 'number',
          spin: 'number',
          give: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (id, assets, input, x, y = x, options = {}) =>
    op2(assets, x, y, options),
});
