import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const cut2 = (assets, shape, tools) => {
  return shape.rewrite((shape, descend) => {
    return shape.derive({
      geometry: shape.geometry && cgal.Cut2(assets, shape, tools),
      shapes: descend(),
    });
  });
};

export const cut3 = (assets, shape, tools) => {
  return shape.rewrite((shape, descend) => {
    return shape.derive({
      geometry: shape.geometry && cgal.Cut3(assets, shape, tools),
      shapes: descend(),
    });
  });
};
