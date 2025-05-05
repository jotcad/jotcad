import './transform.js';

import { Point } from './point.js';
import { Link } from './link.js';
import { cgal } from './getCgal.js';
import { shape } from './shape.js';

const unitArc2 = (assets, sides) => {
  const cursor = Point(assets, 1, 0, 0);
  const points = [];
  for (let nth = 0; nth < sides; nth++) {
    points.push(cursor.rotateZ(`${nth}/${sides}`, nth / sides));
  }
  return Link(assets, points, true, false);
};

export const ArcSlice2 = (assets, arc, start, end) =>
  shape({ geometry: cgal.ArcSlice2(assets, arc, start, end) });

export const Arc2 = (assets, [x0, y0], [x1, y1], sides=16, start=0, end=1, spin=0) => {
  let arc = unitArc2(assets, sides).rotateZ(spin);
  if (start !== 0 || end !== 1) {
    arc = ArcSlice2(assets, arc, start, end);
  }
  return arc.scale(x1 -x0, y1 - y0).move(-x0, -y0);
};
