import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const fill2 = (assets, shapes, holes = false) =>
  makeShape({ geometry: cgal.Fill2(assets, shapes, holes) });

export const fill3 = (assets, shape) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.Fill3(assets, shape),
      shapes: descend(),
    })
  );
