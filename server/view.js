import { Op } from '@jotcad/op';
import { renderPng } from '@jotcad/geometry';

export const view = Op.registerOp({
  name: 'view',
  spec: [null, ['vector3', ['options', { edge: 'boolean' }]], 'shape'],
  code: async (id, assets, input, position, { edge = true } = {}) => {
    return input;
  },
});
