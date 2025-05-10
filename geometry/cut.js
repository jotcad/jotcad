import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const cut = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Cut(assets, shape, tools),
      shapes: descend(),
    })
  );
