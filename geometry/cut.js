import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const cut = (assets, shape, tools) => {
  return shape.rewrite((shape, descend) => {
    return shape.derive({
      geometry: shape.geometry && cgal.Cut(assets, shape, tools),
      shapes: descend(),
    });
  });
};
