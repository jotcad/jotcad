import { Op, predicateValueHandler } from '@jotcad/op';

import { isArray } from './arraySpec.js';
import { isNumber } from './numberSpec.js';

export const isVector3 = (value) =>
  isArray(value) && value.length === 3 && value.every(isNumber);

Op.registerSpecHandler(predicateValueHandler('vector3', isVector3));
