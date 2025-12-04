import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';
import { withAssets } from './assets.js';

export const footprint = (shape) =>
  withAssets((assets) => {
    if (!shape) {
      throw new Error('Footprint requires a shape as input.');
    }
    const geometryId = cgal.footprint(assets, shape);
    return makeShape({ geometry: geometryId });
  });
