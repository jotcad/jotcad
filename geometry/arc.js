import './transform.js';

import { Link } from './link.js';
import { Point } from './point.js';
import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

// Determines the number of sides required for a circle of diameter such that deviation does not exceed tolerance.
// See: https://math.stackexchange.com/questions/4132060/compute-number-of-regular-polgy-sides-to-approximate-circle-to-defined-precision

// For ellipses, use the major diameter for a convervative result.

export const zag = (diameter, tolerance = 0.1) => {
  const r = diameter / 2;
  const k = tolerance / r;
  const s = Math.ceil(Math.PI / Math.sqrt(k * 2));
  return s;
};

const unitArc2 = (assets, sides) => {
  const cursor = Point(assets, -0.5, 0, 0);
  const points = [];
  for (let nth = 0; nth < sides; nth++) {
    points.push(cursor.rotateZ(`${nth}/${sides}`, nth / sides));
  }
  return Link(assets, points, true, false);
};

export const ArcSlice2 = (assets, arc, start, end) =>
  makeShape({ geometry: cgal.ArcSlice2(assets, arc, start, end) });

export const Arc2 = (
  assets,
  [x0, y0],
  [x1, y1],
  sides = undefined,
  start = 0,
  end = 1,
  spin = 0
) => {
  if (sides === undefined) {
    sides = zag(Math.max(x1 - x0, y1 - y0));
  }
  let arc = unitArc2(assets, sides).rotateZ(spin);
  if (start !== 0 || end !== 1) {
    arc = ArcSlice2(assets, arc, start, end);
  }
  return arc
    .move('1/2', '1/2', '0', 0.5, 0.5, 0.5, 0)
    .scale(x1 - x0, y1 - y0)
    .move(x0, y0);
};
