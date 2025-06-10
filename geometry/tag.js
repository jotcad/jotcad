import { makeGroup } from './shape.js';
import { rewrite } from './rewrite.js';

export const setTag = (shape, name, value) =>
  shape.rewrite((shape, descend) =>
    shape.derive({
      tags: { ...shape.tags, [name]: value },
      shapes: descend(),
    })
  );

export const getTagValues = (shape, name) => {
  const values = new Set();
  shape.walk((shape, descend) => {
    const value = shape.tags?.[name];
    if (value !== undefined) {
      values.add(value);
    }
    descend();
  });
  return [...values];
};

export const getShapesByTag = (shape, name, value) =>
  makeGroup(shape.get((shape) => shape.tags?.[name] === value));
