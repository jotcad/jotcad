import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const And = Op.registerOp({
  name: 'And',
  spec: ['shape', ['shapes'], 'shape'],
  code: (assets, input, shapes) => makeShape({ shapes }),
});

export const and = Op.registerOp({
  name: 'and',
  spec: ['shape', ['shapes'], 'shape'],
  code: (assets, input, shapes) => makeShape({ shapes: [input, ...shapes] }),
});
