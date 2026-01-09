import { cgal } from './getCgal.js';

export const smooth = (
  assets,
  shape,
  polylines,
  radius = 1.0,
  angleThreshold = 1.0,
  resolution = 4.0,
  skipFairing = false,
  skipRefine = false,
  fairingContinuity = 1
) => {
  const polylineList = Array.isArray(polylines) ? polylines : [polylines];
  return shape.rewrite((shape, descend) =>
    shape.derive({
      geometry:
        shape.geometry &&
        cgal.Smooth(
          assets,
          shape,
          polylineList,
          radius,
          angleThreshold,
          resolution,
          skipFairing,
          skipRefine,
          fairingContinuity
        ),
      shapes: descend(),
    })
  );
};
