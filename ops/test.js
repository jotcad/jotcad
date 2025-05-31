import { Op } from '@jotcad/op';
import { test as op } from '@jotcad/geometry';

export const test = Op.registerOp({
  name: 'test',
  spec: ['shape', [['flags', ['si']], 'string'], 'shape'],
  code: (assets, input, flags, note) => {
    if (op(assets, input, flags)) {
      throw Error(`test: ${note}`);
    }
    return input;
  },
});
