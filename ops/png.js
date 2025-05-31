import { Op } from '@jotcad/op';
import { renderPng } from '@jotcad/geometry';
import { writeFile } from './fs.js';

export const png = Op.registerOp({
  name: 'png',
  spec: [
    null,
    ['string', 'vector3', ['options', { edge: 'boolean' }]],
    'shape',
  ],
  code: async (assets, input, path, position, { edge = true } = {}) => {
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
