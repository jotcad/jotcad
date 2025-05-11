import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const clip = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Clip(assets, shape, tools),
      shapes: descend(),
    })
  );
