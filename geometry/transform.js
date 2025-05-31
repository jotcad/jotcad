import { Shape } from './shape.js';

export const compose = (a, b) => {
  if (b === undefined) {
    return a;
  } else if (a === undefined) {
    return b;
  } else {
    return [a, b];
  }
};

export const makeIdentity = () => undefined;

export const makeInvert = () => 'i';

export const makeMatrix = (a, b, c, d, e, f, g, h, i, j, k, l, m) =>
  `m ${a} ${b} ${c} ${d} ${e} ${f} ${g} ${h} ${i} ${j} ${k} ${l} ${m}`;

export const makeRotateX = (t, it = t) => `x ${t} ${it}`;

export const makeRotateY = (t, it = t) => `y ${t} ${it}`;

export const makeRotateZ = (t, it = t) => `z ${t} ${it}`;

export const makeScale = (x, y, z = 1, ix = x, iy = y, iz = z) => {
  if (x === 1 && y === 1 && z === 1) {
    return makeIdentity();
  } else {
    return `s ${x} ${y} ${z} ${ix} ${iy} ${iz}`;
  }
};

export const makeMove = (x = 0, y = 0, z = 0, ix = x, iy = y, iz = z) => {
  if (x === 0 && y === 0 && z === 0) {
    return makeIdentity();
  } else {
    return `t ${x} ${y} ${z} ${ix} ${iy} ${iz}`;
  }
};

function transform(tf) {
  return this.rewrite((shape, descend) =>
    shape.derive({
      shapes: descend(),
      tf: compose(tf, shape.tf),
    })
  );
}

function invert() {
  return this.transform(makeInvert());
}

function rotateX(t = 0, it = t) {
  return this.transform(makeRotateX(t, it));
}

function rotateY(t = 0, it = t) {
  return this.transform(makeRotateY(t, it));
}

function rotateZ(t = 0, it = t) {
  return this.transform(makeRotateZ(t, it));
}

function scale(x = 1, y = 1, z = 1, ix = x, iy = y, iz = z) {
  return this.transform(makeScale(x, y, z, ix, iy, iz));
}

function move(x = 0, y = 0, z = 0, ix = x, iy = y, iz = z) {
  return this.transform(makeMove(x, y, z, ix, iy, iz));
}

Shape.prototype.transform = transform;
Shape.prototype.invert = invert;
Shape.prototype.rotateX = rotateX;
Shape.prototype.rotateY = rotateY;
Shape.prototype.rotateZ = rotateZ;
Shape.prototype.scale = scale;
Shape.prototype.move = move;
