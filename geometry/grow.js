import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const grow = (assets, shape, tools) => {
  if (!shape) {
    throw new Error('Grow requires a shape as input.');
  }
  const geometryId = cgal.Grow(assets, shape, tools);
  return makeShape({ geometry: geometryId });
};
