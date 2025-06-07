import { cgal } from './getCgal.js';

export const extrude2 = (assets, shape, top, bottom) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Extrude2(assets, shape, top, bottom),
      shapes: descend(),
    })
  );

export const extrude3 = (assets, shape, top, bottom) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Extrude3(assets, shape, top, bottom),
      shapes: descend(),
    })
  );
