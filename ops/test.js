import { Op } from '@jotcad/op';
import { test as op } from '@jotcad/geometry';

export const test = Op.registerOp(
  'test',
  ['shape', [['flags', ['si']], 'string'], 'shape'],
  (assets, input, flags, note) => {
    if (op(assets, input, flags)) {
      throw Error(`test: ${note}`);
    }
    return input;
  }
);
