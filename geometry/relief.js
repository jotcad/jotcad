import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const relief = (
  assets,
  points,
  {
    rows = 10,
    cols = 10,
    mapping = 'planar',
    subdivisions = 4,
    closedU = false,
    closedV = false,
    targetEdgeLength = 0,
  } = {}
) => {
  const target = makeShape();
  cgal.Relief(
    assets,
    [].concat(points),
    rows,
    cols,
    mapping,
    subdivisions,
    closedU,
    closedV,
    targetEdgeLength,
    target
  );
  return target;
};
