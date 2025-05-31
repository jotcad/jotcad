import { Op, specEquals } from '@jotcad/op';

import { isArray } from './arraySpec.js';
import { nth } from './nth.js';

Op.registerSpecHandler((spec) => {
  return (
    ((isArray(spec) && spec[0] === 'shape') || spec === 'shape') &&
    ((spec, caller, args, rest) => {
      let op;
      if (isArray(spec)) {
        op = spec[1]();
      }
      let result;
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(arg.getOutputType(), spec)) {
          let value = arg.$chain(op);
          result = value;
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
  );
});
