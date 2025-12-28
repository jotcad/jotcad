import { cgal } from './getCgal.js';

export const close3 = (assets, shape) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Close3(assets, shape),
      shapes: descend(),
    })
  );
