import { Op } from '@jotcad/op';
import { renderPng } from '@jotcad/geometry';
import { writeFile } from 'node:fs/promises';

export const png = Op.registerOp(
  'png',
  [null, [], 'shape'],
  async (assets, input, path, position) => {
    const image = await renderPng(assets, input, { view: { position }, width: 512, height: 512 });
    await writeFile(path, Buffer.from(image));
    return input;
  }
);
