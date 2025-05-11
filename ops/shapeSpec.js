import { Op, specEquals } from '@jotcad/op';

Op.registerSpecHandler(
  (spec) =>
    spec === 'shape' &&
    ((spec, input, args, rest) => {
      let result;
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op && specEquals(arg.getOutputType(), spec)) {
          result = arg;
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
);
