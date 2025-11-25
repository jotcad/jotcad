import './flagsSpec.js';
import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { test as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const test = registerOp({
  name: 'test',
  spec: ['shape', [['flags', ['si']], 'string'], 'shape'],
  code: (id, session, input, flags, note) => {
    // Changed assets to session
    if (op(session.assets, input, flags)) {
      // Use session.assets
      throw Error(`test: ${note}`);
    }
    return input;
  },
});
