import { cgal } from './getCgal.js';
import { makeShape } from './shape.js'; // Import makeShape

export const rule = (assets, shapes, options = {}) => {
  console.log('geometry/rule.js: shapes:', JSON.stringify(shapes, null, 2));
  return makeShape({
    geometry: cgal.Rule(assets, shapes, options),
  });
};
