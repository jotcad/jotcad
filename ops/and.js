import { Op } from '@jotcad/op';
import { makeShape } from '@jotcad/geometry';

export const And = Op.registerOp(
  'And',
  ['shape', ['shapes'], 'shape'],
  (assets, input, shapes) => {
    console.log(`QQ/And: shapes=${JSON.stringify(shapes)}`);
    const result = makeShape({ shapes });
    console.log(`QQ/And: result=${JSON.stringify(shapes)}`);
    return result;
  }
);

export const and = Op.registerOp(
  'and',
  ['shape', ['shapes'], 'shape'],
  (assets, input, shapes) => makeShape({ shapes: [input, ...shapes] })
);
