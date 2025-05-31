import { Op } from '@jotcad/op';
import { Arc2 as op2 } from '@jotcad/geometry';

export const Arc2 = Op.registerOp({
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
  code: (assets, input, x, y = x, options = {}) => op2(assets, x, y, options),
});
