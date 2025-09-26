import { Op, specEquals } from '@jotcad/op';

Op.registerSpecHandler((spec) => {
  return (
    spec === 'op' &&
    ((spec, caller, args, rest) => {
      let op;
      let result;
      while (args.length >= 1) {
        const arg = args.shift();
        if (arg instanceof Op) {
          result = arg;
          break;
        }
        rest.push(arg);
      }
      rest.push(...args);
      return result;
    })
  );
});
