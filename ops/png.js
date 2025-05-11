import { Op } from '@jotcad/op';
import { renderPng } from '@jotcad/geometry';
import { writeFile } from './fs.js';

export const png = Op.registerOp(
  'png',
  [null, ['string', 'vector3'], 'shape'],
  async (assets, input, path, position) => {
    const width = 512;
    const height = 512;
    const image = await renderPng(assets, input, {
      view: { position },
      width,
      height,
    });
    const data = Buffer.from(image);
    await writeFile(path, data);
    return input;
  }
);
