import { cgal } from './getCgal.js';

export const join = (assets, shape, tools) =>
  shape.derive({
    geometry: cgal.Join(assets, shape, [shape, ...tools]),
    shapes: null,
  });
