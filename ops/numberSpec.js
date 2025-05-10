import { Op, predicateValueHandler } from '@jotcad/op';

export const isNumber = (value) => typeof value === 'number';

Op.registerSpecHandler(predicateValueHandler('number', isNumber));
