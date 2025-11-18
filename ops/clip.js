import './shapeSpec.js';
import './shapesSpec.js';
import { clip as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const clip = registerOp({
  name: 'clip',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, tools) => op(assets, input, tools),
});
