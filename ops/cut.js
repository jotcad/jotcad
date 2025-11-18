import './shapeSpec.js';
import './shapesSpec.js';
import { cut as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const cut = registerOp({
  name: 'cut',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, tools) => {
    console.log(`QQ/cut: input=${input}`);
    return op(assets, input, tools);
  }
});
