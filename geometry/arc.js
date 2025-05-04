import { rotateZ, scale, translate, transform } from './transform.js';

import { Point } from './point.js';
import { Link } from './link.js';
import { assets } from './assets.js';
import { cgal } from './getCgal.js';
import { shape } from './shape.js';

const unitArc2 = (sides) => {
  const cursor = Point(1, 0, 0);
  const points = [];
  for (let nth = 0; nth < sides; nth++) {
    points.push(transform(rotateZ(`${nth}/${sides}`, nth / sides), cursor));
  }
  return Link(points, true, false);
};

export const ArcSlice2 = (arc, start, end) =>
  shape({ geometry: cgal.ArcSlice2(assets, arc, start, end) });

export const Arc2 = ([x0, y0], [x1, y1], sides=16, start=0, end=1, spin=0) => {
  let arc = transform(rotateZ(spin), unitArc2(sides));
  if (start !== 0 || end !== 1) {
    arc = ArcSlice2(arc, start, end);
  }
  return transform(translate(-x0, -y0), transform(scale(x1 - x0, y1 - y0), arc));
};
