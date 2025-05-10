import { makeShape } from './shape.js';
import { rewrite } from './rewrite.js';

export const tag = (shape, name, value) =>
  rewrite(shape, (shape, rewrite) => {
    return makeShape({
      tags: { ...shape.tags, [name]: value },
      shapes: shape.shapes?.map(rewrite),
      geometry: shape.geometry,
      tf: shape.tf,
    });
  });
