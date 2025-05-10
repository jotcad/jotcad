import { Op } from '@jotcad/op';
import { readFile } from 'node:fs/promises';

export const Load = Op.registerOp(
  'Load',
  [null, ['string'], 'shape'],
  (path) => () => readFile(path)
);
