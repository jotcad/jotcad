import './opSpec.js';
import './shapeSpec.js';

import { registerOp } from './op.js';

export const at = registerOp({
  name: 'at',
  spec: ['shape', ['shape', 'op'], 'shape'],
  args: (input, shape, op) => {
    return [shape, shape.revert().$chain(op)];
  },
  code: (id, session, input, original, transformed) => {
    // Changed assets to session
    const map = new Map();
    const walk = (original, transformed) => {
      if (original.geometry) {
        map.set(original, transformed);
      }
      if (original.shapes) {
        for (let nth = 0; nth < original.shapes.length; nth++) {
          walk(original.shapes[nth], transformed.shapes[nth]);
        }
      }
    };
    walk(original, transformed);
    const result = input.rewrite((original, descend) => {
      const transformed = map.get(original);
      if (transformed) {
        return transformed.transform(original.tf);
      } else {
        return original.derive({
          shapes: descend(),
        });
      }
    });
    return result;
  },
});
