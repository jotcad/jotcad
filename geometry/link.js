import { cgal } from './getCgal.js';
import { assets } from './assets.js';
import { shape } from './shape.js';

export const Link = (points, close=false, reverse=false) =>
  shape({ geometry: cgal.Link(assets, points, close, reverse) });
