import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const And = Op.registerOp(
  'And',
  ['shape', ['shapes'], 'shape'],
  (assets, input, shapes) => makeShape({ shapes })
);

export const and = Op.registerOp(
  'and',
  ['shape', ['shapes'], 'shape'],
  (assets, input, shapes) => makeShape({ shapes: [input, ...shapes] })
);
