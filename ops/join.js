import { join as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const join = registerOp({
  name: 'join',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, tools) => op(assets, input, tools),
});
