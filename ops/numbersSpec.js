import { Op, specEquals } from '@jotcad/op';

export const isNumber = (value) => typeof value === 'number';

Op.registerSpecHandler(
  (spec) =>
    spec === 'numbers' &&
    ((spec, caller, args, rest) => {
      const results = [];
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(arg.getOutputType(), 'numbers')) {
          results.push(arg);
          continue;
        } else if (arg instanceof Object) {
          let { ge, gt, by, le, lt } = arg;
          if (by === undefined || by === 0) {
            rest.push(arg);
            continue;
          }
          if (ge === undefined && gt === undefined) {
            // Start at zero, by default.
            ge = 0;
          }
          if (le === undefined && lt === undefined) {
            lt = 0;
          }
          let cursor;
          if (ge !== undefined) {
            cursor = ge;
          } else {
            cursor = gt + by;
          }
          if (lt !== undefined) {
            while (cursor < lt) {
              results.push(cursor);
              cursor += by;
            }
          } else {
            while (cursor <= le) {
              results.push(cursor);
              cursor += by;
            }
          }
          continue;
        } else if (typeof arg == 'number') {
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
