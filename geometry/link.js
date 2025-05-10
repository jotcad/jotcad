import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const Link = (assets, points, close = false, reverse = false) =>
  makeShape({ geometry: cgal.Link(assets, points, close, reverse) });
