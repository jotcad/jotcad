import { Op, specEquals } from '@jotcad/op';
import { isShape } from '@jotcad/geometry';

Op.registerSpecHandler(
  (spec) =>
    spec === 'shapes' &&
    ((spec, caller, args, rest) => {
      const results = [];
      while (args.length >= 1) {
        const arg = args.shift();
        console.log(
          'shapesSpec: checking arg=',
          arg ? arg.name || typeof arg : 'null'
        );
        if (isShape(arg)) {
          results.push(arg);
          continue;
        } else if (
          arg instanceof Op &&
          specEquals(arg.getOutputType(), 'shape')
        ) {
          results.push(arg);
          continue;
        } else if (arg instanceof Op) {
          console.log(
            'shapesSpec: arg is Op but outputType is',
            arg.getOutputType()
          );
        }
        rest.push(arg);
      }
      rest.push(...args);
      return results;
    })
);
