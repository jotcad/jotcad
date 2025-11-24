import './intervalSpec.js';
import './optionsSpec.js';
import { Orb as op } from '@jotcad/geometry';
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
  code: (id, session, input, x = 1, y = x, z = x, options = {}) => {
    // Changed assets to session
    const { zag } = options; // Extract zag
    return op(session.assets, x, y, z, zag); // Pass x, y, z, zag
  },
});
