import { registerOp } from './op.js';
import { setTag } from '@jotcad/geometry';

export const set = registerOp({
  name: 'set',
  spec: ['shape', ['string', 'string'], 'shape'],
  code: (id, assets, input, name, value) => setTag(input, name, value),
});
