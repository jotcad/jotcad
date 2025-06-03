import { Op } from '@jotcad/op';
import { readFile } from './fs.js';

export const Load = Op.registerOp({
  name: 'Load',
  spec: [null, ['string'], 'shape'],
  code: (path) => () => readFile(path),
});
