import { makeAbsolute } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const absolute = registerOp({
  name: 'absolute',
  spec: ['shape', [], 'shape'],
  code: (id, assets, input) => makeAbsolute(assets, input),
});
