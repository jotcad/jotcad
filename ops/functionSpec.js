import { Op, predicateValueHandler } from '@jotcad/op';

export const isFunction = (value) => typeof value === 'function';

Op.registerSpecHandler(predicateValueHandler('function', isFunction));
