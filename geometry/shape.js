export class Shape {
  constructor({
    geometry = undefined,
    mask = undefined,
    shapes = undefined,
    tags = undefined,
    tf = undefined,
  } = {}) {
    if (geometry) {
      this.geometry = geometry;
    }
    if (mask) {
      this.mask = mask;
    }
    if (shapes) {
      this.shapes = shapes;
    }
    if (tags) {
      this.tags = tags;
    }
    if (tf) {
      this.tf = tf;
    }
  }

  derive({
    geometry = this.geometry,
    mask = this.mask,
    shapes = this.shapes,
    tags = this.tags,
    tf = this.tf,
  }) {
    return makeShape({ geometry, mask, shapes, tags, tf });
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

  flatten() {
    const shapes = [];
    this.walk((shape, descend) => {
      if (shape.geometry) {
        shapes.push(shape.derive({ shapes: null }));
      }
      descend();
    });
    return shapes;
  }

  nth(...indices) {
    const shapes = this.flatten();
    const results = [];
    for (const index of indices) {
      results.push(shapes[index]);
    }
    return results;
  }

  get(predicate) {
    const results = [];
    const walk = (shape) => {
      if (predicate(shape)) {
        results.push(shape);
      } else if (shape.shapes) {
        for (const subShape of shape.shapes) {
          walk(subShape);
        }
      }
    };
    walk(this);
    return results;
  }

  static fromPlain(plain) {
    if (!plain) return plain;
    const shape = new Shape();
    Object.assign(shape, plain);
    if (shape.shapes) {
      shape.shapes = shape.shapes.map((s) => Shape.fromPlain(s));
    }
    return shape;
  }
}

export const makeShape = (args) => new Shape(args);

export const makeGroup = (shapes) => makeShape({ shapes });

export const isShape = (value) => value instanceof Shape;
