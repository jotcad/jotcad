import { cgal } from './getCgal.js';

export const makeAbsolute = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.MakeAbsolute(assets, shape),
      shapes: descend(),
    })
  );
