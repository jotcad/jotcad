import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const fill = (assets, shapes, holes = false) =>
  makeShape({ geometry: cgal.Fill(assets, shapes, holes) });
