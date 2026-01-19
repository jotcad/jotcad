import './functionSpec.js';
import { registerOp } from './op.js';

export const debug = registerOp({
  name: 'debug',
  effect: true,
  spec: ['shape', ['function'], 'shape'],
  code: (id, session, input, predicate) => {
    predicate(input);
    return input;
  },
});
