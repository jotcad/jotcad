import { Op, specEquals } from '@jotcad/op';

import { nth } from './nth.js';

Op.registerSpecHandler(
  (spec) =>
    spec === 'shape' &&
    ((spec, caller, args, rest) => {
      let result;
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(arg.getOutputType(), spec)) {
          const value = arg.nth(0);
          value.caller = caller;
          result = value;
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
);
