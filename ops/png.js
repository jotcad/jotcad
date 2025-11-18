import './stringSpec.js';
import './vectorSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import { registerOp } from './op.js';
import { renderPng } from '@jotcad/geometry';
import { writeFile } from './fs.js';

export const png = registerOp({
  name: 'png',
  effect: true,
  spec: [
    null,
    ['string', 'vector3', ['options', { edge: 'boolean' }]],
    'shape',
  ],
  code: async (id, assets, input, path, position, { edge = true } = {}) => {
    const width = 512;
    const height = 512;
    const image = await renderPng(assets, input, {
      view: { position },
      width,
      height,
      doOutlineEdges: edge,
    });
    const data = Buffer.from(image);
    console.log(`QQ/png: path=${path}`);
    await writeFile(path, data, { id });
    return input;
  },
});
