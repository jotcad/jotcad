import { cgal } from './cgal';
import { makeShape } from './shape';
import { withAssets } from './assets';

export const footprint = (shape) => withAssets((assets) => {
  if (!shape) {
    throw new Error('Footprint requires a shape as input.');
  }
  const geometryId = cgal.footprint(assets, shape);
  return makeShape({ geometry: geometryId });
});