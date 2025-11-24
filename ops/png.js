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
  code: async (id, session, input, path, position, { edge = true } = {}) => {
    console.log(`ops/png.js: Writing file to relative path: ${path}`);
    const width = 512;
    const height = 512;
    const image = await renderPng(session.assets, input, {
      view: { position },
      width,
      height,
      doOutlineEdges: edge,
    });
    const data = Buffer.from(image);
    await writeFile(path, data, { id });
    return input;
  },
});
