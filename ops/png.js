import { registerOp } from './op.js';
import { renderPng } from '@jotcad/geometry';
import { writeFile } from './fs.js';

export const png = registerOp({
  name: 'png',
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
    await writeFile(path, data);
    return input;
  },
});
