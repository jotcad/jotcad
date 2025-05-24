import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const simplify = (assets, shape, { faceCount } = {}) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Simplify(assets, shape, faceCount),
      shapes: descend(),
    })
  );
