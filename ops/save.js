import './shapeSpec.js';
import './stringSpec.js';
import { registerOp } from './op.js';
import { writeFile } from './fs.js';

export const save = registerOp({
  name: 'save',
  spec: ['shape', ['string'], 'shape'],
  code: async (id, assets, input, path) => {
    await writeFile(path, JSON.stringify({ assets, input }));
    return input;
  },
});
