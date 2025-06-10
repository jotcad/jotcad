import { test as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const test = registerOp({
  name: 'test',
  spec: ['shape', [['flags', ['si']], 'string'], 'shape'],
  code: (id, assets, input, flags, note) => {
    if (op(assets, input, flags)) {
      throw Error(`test: ${note}`);
    }
    return input;
  },
});
