import { Op, specEquals } from '@jotcad/op';

import { isArray } from './arraySpec.js';
import { registerOp } from './op.js';

export const isObject = (value) => value !== null && typeof value === 'object';

const isKeysConforming = (schema, options) => {
  for (const key of Object.keys(options)) {
    if (!schema.hasOwnProperty(key)) {
      return false;
    }
  }
  return true;
};

Op.registerSpecHandler(
  (spec) =>
    isArray(spec) &&
    spec[0] === 'options' &&
    ((spec, caller, args, rest) => {
      let result;
      const schema = spec[1];
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(Op.getOutputType(), spec)) {
          // TODO: Handle post-validation.
          result = arg;
          break;
        } else if (isObject(arg) && isKeysConforming(schema, arg)) {
          const resolved = {};
          for (const key of Object.keys(arg)) {
            const [value] = Op.destructure(
              'options',
              [null, [schema[key]], null],
              caller,
              [arg[key]]
            );
            resolved[key] = value;
          }
          result = resolved;
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
);
