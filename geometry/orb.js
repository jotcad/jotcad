import './transform.js'; // Added this line

import { buildCorners, computeMiddle, computeScale } from './corners.js';

import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

const DEFAULT_ORB_ZAG = 1;

// Utility function for vector scaling (moved from original JSxCAD vector.js)
const scaleVector = (s, v) => v.map((c) => s * c);

export const Orb = (assets, x = 1, y = x, z = x, zag = DEFAULT_ORB_ZAG) => {
  console.log(`QQ/geometry/Orb`);

  const [c1, c2] = buildCorners(x, y, z);
  const scale = scaleVector(0.5, computeScale(c1, c2));
  const middle = computeMiddle(c1, c2);
  const radius = Math.max(...scale);
  const tolerance = zag / radius;

  const angularBound = 30;
  const radiusBound = tolerance;
  const distanceBound = tolerance;

  const geometryId = cgal.MakeOrb(
    assets,
    angularBound,
    radiusBound,
    distanceBound
  );

  // Create a Shape object and apply transformations
  return makeShape({ geometry: geometryId })
    .scale(scale[0], scale[1], scale[2])
    .move(middle[0], middle[1], middle[2]);
};
