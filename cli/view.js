import { Op } from '@jotcad/op';
import { renderPng } from '@jotcad/geometry';

export const view = Op.registerOp({
  name: 'view',
  spec: [null, ['vector3', ['options', { edge: 'boolean' }]], 'shape'],
  code: async (assets, input, position, { edge = true } = {}) => {
    const width = 512;
    const height = 512;
    const image = await renderPng(assets, input, {
      view: { position },
      width,
      height,
      doOutlineEdges: edge,
    });
    const data = Buffer.from(image);
    const b64 = data.toString('base64');
    for (let i = 0; i < b64.length; i += 4096) {
      const chunk = b64.substring(i, i + 4096);
      console.log(
        `\x1b_Ga=T,q=2,f=100,m=${
          chunk.length >= 4096 ? '1' : '0'
        },s=${width},v=${height};${chunk}\x1b\\`
      );
    }
    return input;
  },
});
