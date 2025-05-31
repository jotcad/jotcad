import { Op, specEquals } from '@jotcad/op';

Op.registerSpecHandler(
  (spec) =>
    spec === 'shapes' &&
    ((spec, caller, args, rest) => {
      const results = [];
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(arg.getOutputType(), 'shape')) {
          results.push(arg);
          continue;
        } else if (arg === undefined) {
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return results;
    })
);
