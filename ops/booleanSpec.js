import { Op, predicateValueHandler } from '@jotcad/op';

export const isBoolean = (value) => typeof value === 'boolean';

Op.registerSpecHandler(predicateValueHandler('boolean', isBoolean));
