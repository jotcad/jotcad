export class Shape {
  constructor({
    shapes = undefined,
    geometry = undefined,
    tags = undefined,
    tf = undefined,
  } = {}) {
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

  derive({
    geometry = this.geometry,
    shapes = this.shapes,
    tags = this.tags,
    tf = this.tf,
  }) {
    return makeShape({ geometry, shapes, tags, tf });
  }

  walk(op) {
    const descend = () => {
      if (this.shapes) {
        for (const shape of this.shapes) {
          shape.walk(op);
        }
      }
    };
    op(this, descend);
  }

  rewrite(op) {
    const descend = () => {
      let rewrittenShapes;
      if (this.shapes) {
        rewrittenShapes = [];
        for (const shape of this.shapes) {
          rewrittenShapes.push(shape.rewrite(op));
        }
      }
      return rewrittenShapes;
    };
    return op(this, descend);
  }
}

export const makeShape = (args) => new Shape(args);

export const makeGroup = (shapes) => makeShape({ shapes });
