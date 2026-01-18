import { cgal } from './getCgal.js';

export const edges = (assets, shape, angleThreshold = 0.1) => {
  return shape.rewrite((shape, descend) =>
    shape.derive({
      geometry:
        shape.geometry && cgal.ExtractEdges(assets, shape, angleThreshold),
      shapes: descend(),
    })
  );
};
