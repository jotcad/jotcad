import { cgal } from './getCgal.js';

export const clipOpen = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.ClipOpen(assets, shape, tools),
      shapes: descend(),
    })
  );
