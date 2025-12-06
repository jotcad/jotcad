import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const grow = (assets, shape, tools) => {
  if (!shape) {
    throw new Error('Grow requires a shape as input.');
  }
  return shape.rewrite((shape, descend) => {
    console.log(
      `QQQ/grow: Grow.shape: ${shape.geometry && JSON.stringify(shape)}`
    );
    return shape.derive({
      geometry: shape.geometry && cgal.Grow(assets, shape, tools),
      shapes: descend(),
    });
  });
};
