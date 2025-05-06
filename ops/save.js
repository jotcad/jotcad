import { Op } from '@jotcad/op';
import { writeFile } from 'node:fs/promises';

export const save = Op.registerOp(
  'save',
  ['shape', ['string'], 'shape'],
  (assets, input, path) => writeFile(`shape/${path}`, input)
);
