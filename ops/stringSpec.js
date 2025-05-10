import { Op, predicateValueHandler } from '@jotcad/op';

export const isString = (value) => typeof value === 'string';

Op.registerSpecHandler(predicateValueHandler('string', isString));
