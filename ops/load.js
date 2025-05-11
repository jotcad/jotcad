import { Op } from '@jotcad/op';
import { readFile } from './fs.js';

export const Load = Op.registerOp(
  'Load',
  [null, ['string'], 'shape'],
  (path) => () => readFile(path)
);
