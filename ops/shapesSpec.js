import { Op, specEquals } from '@jotcad/op';
import { isShape } from '@jotcad/geometry';

Op.registerSpecHandler(
  (spec) =>
    spec === 'shapes' &&
    ((spec, caller, args, rest) => {
      const results = [];
      const process = (arg) => {
        if (isShape(arg)) {
          results.push(arg);
          return true;
        } else if (
          arg instanceof Op &&
          specEquals(arg.getOutputType(), 'shape')
        ) {
          results.push(arg);
          return true;
        } else if (Array.isArray(arg)) {
          for (const item of arg) {
            process(item);
          }
          return true;
        }
        return false;
      };
      while (args.length >= 1) {
        const arg = args.shift();
        if (!process(arg)) {
          rest.push(arg);
        }
      }
      rest.push(...args);
      return results;
    })
);
