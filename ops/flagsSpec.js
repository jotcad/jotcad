import { Op, specEquals } from '@jotcad/op';

import { isArray } from './arraySpec.js';
import { registerOp } from './op.js';

Op.registerSpecHandler(
  (spec) =>
    isArray(spec) &&
    spec[0] === 'flags' &&
    ((spec, caller, args, rest) => {
      const results = {};
      const schema = spec[1];
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(Op.getOutputType(), spec)) {
          // TODO: Handle post-validation.
          result = arg;
          continue;
        } else if (schema.includes(arg)) {
          results[arg] = true;
          continue;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return results;
    })
);
