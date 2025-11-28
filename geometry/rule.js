import { cgal } from './getCgal.js';
import { makeShape } from './shape.js'; // Import makeShape

export const rule = (assets, from_shape, to_shape, options = {}) =>
  makeShape({ geometry: cgal.Rule(assets, from_shape, to_shape, options) });
