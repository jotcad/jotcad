import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';

export const texture = (
  assets,
  shape,
  textureShapes,
  { rows = 50, cols = 50, mapping = 'planar', scale = 1, strategy = 'max' } = {}
) => {
  return cgal.Texture(
    assets,
    shape,
    [].concat(textureShapes),
    rows,
    cols,
    mapping,
    scale,
    strategy
  );
};
