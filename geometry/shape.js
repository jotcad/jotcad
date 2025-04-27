export const shape = ({ shapes=undefined, geometry=undefined, tags=undefined, tf=undefined } = {}) => {
  const shape = {};
  if (shapes !== undefined) {
    shape.shapes = shapes;
  }
  if (geometry !== undefined) {
    shape.geometry = geometry;
  }
  if (tags !== undefined) {
    shape.tags = tags;
  }
  if (tf !== undefined) {
    shape.tf = tf;
  }
  return shape;
};
