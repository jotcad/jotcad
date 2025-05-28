import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const test = (assets, shape, { si = false } = {}) => {
  let failed = false;
  shape.walk((shape, descend) => {
    if (shape.geometry) {
      if (cgal.Test(assets, shape, si)) {
        failed = true;
      }
    }
    descend();
  });
  return failed;
};
