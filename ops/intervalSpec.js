import { Op } from '@jotcad/op';
import { isArray } from './arraySpec.js';
import { isNumber } from './numberSpec.js';

export const isIntervalLike = (value) =>
  isNumber(value) ||
  (isArray(value) &&
    isNumber(value[0]) &&
    (isNumber(value[1]) || value[1] === undefined));

export const normalizeInterval = (value) => {
  if (isNumber(value)) {
    value = [value / 2, value / -2];
  }
  const [a = 0, b = 0] = value;
  if (typeof a !== 'number') {
    throw Error(
      `normalizeInterval expected number but received ${a} of type ${typeof a}`
    );
  }
  if (typeof b !== 'number') {
    throw Error(
      `normalizeInterval expected number but received ${b} of type ${typeof b}`
    );
  }
  return a < b ? [a, b] : [b, a];
};

Op.registerSpecHandler(
  (spec) =>
    spec === 'interval' &&
    ((spec, caller, args, rest) => {
      let result;
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && arg.getOutputType() === 'interval') {
          // TODO: Handle deferred normalization.
          result = arg;
          break;
        } else if (
          isIntervalLike(arg) ||
          (arg instanceof Op && arg.getOutputType() === 'interval')
        ) {
          result = normalizeInterval(arg);
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
);
