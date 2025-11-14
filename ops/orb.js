import { Orb as op } from '../geometry/orb.js';
import { registerOp } from './op.js';

export const Orb = registerOp({
  name: 'Orb',
  spec: [
    null,
    [
      'interval',
      'interval',
      'interval',
      [
        'options',
        {
          zag: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (id, assets, input, x = 1, y = x, z = x, options = {}) => {
    console.log(`QQ/Orb`);
    const { zag } = options; // Extract zag
    return op(assets, x, y, z, zag); // Pass x, y, z, zag
  },
});