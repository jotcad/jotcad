import { cgal } from './getCgal.js';
import { group } from './shape.js';

export const extrude = (assets, shape, top, bottom) => {
  const ids = [];
  cgal.Extrude(assets, shape, top, bottom, ids);
  const shapes = [];
  for (const id of ids) {
    shapes.push(shape.withGeometry(id));
  }
  return group(shapes);
};
