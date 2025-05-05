import { cgal } from './getCgal.js';
import { shape } from './shape.js';

export const Link = (assets, points, close = false, reverse = false) =>
  shape({ geometry: cgal.Link(assets, points, close, reverse) });
