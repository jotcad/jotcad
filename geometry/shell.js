import { cgal } from './getCgal.js';

export const shell = (
  assets,
  shape,
  inner_offset = 0,
  outer_offset = 1,
  protect = true,
  angle = 30,
  sizing = 1,
  approx = 0.1,
  edge_size = 1
) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      geometry:
        shape.geometry &&
        cgal.Shell(
          assets,
          shape,
          inner_offset,
          outer_offset,
          protect,
          angle,
          sizing,
          approx,
          edge_size
        ),
      shapes: descend(),
    })
  );
