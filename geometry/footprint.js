import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const footprint = (assets, shape) => {
  if (!shape) {
    throw new Error('Footprint requires a shape as input.');
  }
  const geometryId = cgal.footprint(assets, shape);
  return makeShape({ geometry: geometryId });
};
