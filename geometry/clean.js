import { cgal } from './getCgal.js';

export const clean = (
  assets,
  shape,
  angleThreshold = 1.0,
  useAngleConstrained = true,
  regularize = true,
  collapse = true,
  planeDistanceThreshold = 0.1
) => {
  return shape.rewrite((shape, descend) =>
    shape.derive({
      geometry:
        shape.geometry &&
        cgal.Clean(
          assets,
          shape,
          angleThreshold,
          useAngleConstrained,
          regularize,
          collapse,
          planeDistanceThreshold
        ),
      shapes: descend(),
    })
  );
};
