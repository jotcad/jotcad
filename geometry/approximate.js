import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const approximate = (assets, shape, { faceCount, minErrorDrop } = {}) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry:
        shape.geometry &&
        cgal.Approximate(assets, shape, faceCount, minErrorDrop),
      shapes: descend(),
    })
  );
