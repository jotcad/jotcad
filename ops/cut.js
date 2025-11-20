import './shapeSpec.js';
import './shapesSpec.js';
import { cut as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const cut = registerOp({
  name: 'cut',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, tools) =>
    op(assets, input, tools)
});
