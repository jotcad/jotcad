import { cgal } from './getCgal.js';

export const cutOpen = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.CutOpen(assets, shape, tools),
      shapes: descend(),
    })
  );
