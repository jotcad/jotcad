import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const hull = (assets, shapes) =>
  makeShape({
    geometry: cgal.Hull(assets, shapes),
  });
