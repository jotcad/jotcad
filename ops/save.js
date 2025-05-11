import { Op } from '@jotcad/op';
import { writeFile } from './fs.js';

export const save = Op.registerOp(
  'save',
  ['shape', ['string'], 'shape'],
  async (assets, input, path) => {
    await writeFile(path, JSON.stringify(input));
    return input;
  }
);
