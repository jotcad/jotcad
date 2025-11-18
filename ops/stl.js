import './stringSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';

import { fromStl, toStl } from '@jotcad/geometry';
import { readFile, writeFile } from './fs.js';

import { registerOp } from './op.js';

export const Stl = registerOp({
  name: 'Stl',
  spec: [null, ['string', ['options', { format: 'string' }]], 'shape'],
  code: async (id, assets, input, path, { format = 'binary' } = {}) =>
    fromStl(assets, await readFile(path), { format }),
});

export const stl = registerOp({
  name: 'stl',
  spec: ['shape', ['string'], 'shape'],
  code: async (id, assets, input, path) => {
    await writeFile(path, toStl(assets, input));
    return input;
  },
});
