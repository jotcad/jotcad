import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const join = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Join(assets, shape, tools),
      shapes: descend(),
    })
  );
