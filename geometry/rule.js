import { cgal } from './getCgal.js';
import { makeShape } from './shape.js'; // Import makeShape

export const rule = (assets, from_shape, to_shape, options = {}) => {
  console.log(
    'geometry/rule.js: from_shape:',
    JSON.stringify(from_shape, null, 2)
  );
  console.log('geometry/rule.js: to_shape:', JSON.stringify(to_shape, null, 2));
  return makeShape({
    geometry: cgal.Rule(assets, from_shape, to_shape, options),
  });
};
