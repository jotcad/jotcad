import { cgal } from './getCgal.js';
import { makeIdentity } from './transform.js';

export const makeAbsolute = (assets, shape, tools) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry: shape.geometry && cgal.MakeAbsolute(assets, shape),
      shapes: descend(),
      tf: makeIdentity(),
    })
  );
