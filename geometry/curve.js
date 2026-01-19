import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const Curve = (
  assets,
  points,
  { closed = false, resolution = 10 } = {}
) => makeShape({ geometry: cgal.Curve(assets, points, closed, resolution) });
