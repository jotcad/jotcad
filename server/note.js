import '@jotcad/ops';
import { registerOp } from '@jotcad/ops';

export const note = registerOp({
  name: 'note',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, assets, input, tools) => {
    console.log(note);
    return input;
  },
});
