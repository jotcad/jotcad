export class Shape {
  constructor ({ shapes=undefined, geometry=undefined, tags=undefined, tf=undefined } = {}) {
    if (shapes !== undefined) {
      this.shapes = shapes;
    }
    if (geometry !== undefined) {
      this.geometry = geometry;
    }
    if (tags !== undefined) {
      this.tags = tags;
    }
    if (tf !== undefined) {
      this.tf = tf;
    }
  }
};

export const shape = (args) => new Shape(args);
