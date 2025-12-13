import { Op, specEquals } from '@jotcad/op';

export const isNumber = (value) => typeof value === 'number';

export const numbersSpecHandler = (spec, caller, args, rest) => {
  const results = [];
  console.log(`QQ/numbersSpecHandler/0`);

  while (args.length >= 1) {
    const arg = args.shift();
    console.log(`QQ/numbersSpecHandler/0: arg=${JSON.stringify(arg)}`);

    if (arg instanceof Op && specEquals(arg.getOutputType(), 'numbers')) {
      console.log(`QQ/numbersSpecHandler/1`);

      results.push(arg);

      continue;
    } else if (typeof arg == 'object') {
      console.log(`QQ/numbersSpecHandler/2`);

      let { ge, gt, by, le, lt } = arg;

      console.log('numbersSpecHandler: object arg - by:', by);

      if (by === undefined || by === 0) {
        rest.push(arg);

        continue;
      }

      if (ge === undefined && gt === undefined) {
        // Start at zero, by default.

        ge = 0;
      }

      if (le === undefined && lt === undefined) {
        lt = 1;
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
      console.log(`QQ/numbersSpecHandler/3`);

      results.push(arg);

      continue;
    } else if (arg === undefined) {
      console.log(`QQ/numbersSpecHandler/4`);

      break;
    } else {
      console.log(`QQ/numbersSpecHandler!`);
    }

    rest.push(arg);
  }

  rest.push(...args);

  console.log('numbersSpecHandler: returning results:', results);

  return results;
};

Op.registerSpecHandler((spec) => spec === 'numbers' && numbersSpecHandler);
