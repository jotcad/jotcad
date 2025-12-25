// geometry/wrap.js
import { cgal } from './getCgal.js'; // Assuming getCgal.js or getCgalNodeNative.js exports cgal
import { makeShape } from './shape.js';

export const Wrap3 = (assets, shape, alpha = 1.0, offset = 0.0) =>
  makeShape({ geometry: cgal.Wrap3(assets, shape, alpha, offset) });
