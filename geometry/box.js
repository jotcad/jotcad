import { buildCorners } from './corners.js';

const buildFs = () => {
  if (fundamentalShapes === undefined) {
    const f = fill(
      Loop([Point(1, 0, 0), Point(1, 1, 0), Point(0, 1, 0), Point(0, 0, 0)])
    );
    fundamentalShapes = {
      tlfBox: Point(),
      tlBox: Edge([0, 1, 0], [0, 0, 0]),
      tfBox: Edge([0, 0, 0], [1, 0, 0]),
      tBox: f,
      lfBox: Edge([0, 0, 0], [0, 0, 1]),
      lBox: rotateX(rotateZ(rotateY(f, 1 / 4), 1 / 2), -1 / 4),
      fBox: rotateY(rotateZ(rotateX(f, 1 / 4), 1 / 2), -1 / 4),
      box: extrudeAlongZ(f, [[0, 1]]),
    };
  }
  return fundamentalShapes;
};

const makeBox = (corner1, corner2) => {
  const build = () => {
    const fs = buildFs();
    const left = corner2[X];
    const right = corner1[X];

    const front = corner2[Y];
    const back = corner1[Y];

    const bottom = corner2[Z];
    const top = corner1[Z];

    if (top === bottom) {
      if (left === right) {
        if (front === back) {
          return translate(fs.tlfBox, [left, front, bottom]);
        } else {
          return translate(scale(fs.tlBox, [1, back - front, 1]), [
            left,
            front,
            bottom,
          ]);
        }
      } else {
        if (front === back) {
          return translate(scale(fs.tfBox, [right - left, 1, 1]), [
            left,
            front,
            bottom,
          ]);
        } else {
          return translate(scale(fs.tBox, [right - left, back - front, 1]), [
            left,
            front,
            bottom,
          ]);
        }
      }
    } else {
      if (left === right) {
        if (front === back) {
          // return fs.lfBox.sz(top - bottom).move(left, front, bottom);
          return translate(scale(fs.lfBox, [1, 1, top - bottom]), [
            left,
            front,
            bottom,
          ]);
        } else {
          // return fs.lBox.sz(top - bottom).sy(back - front).move(left, front, bottom);
          return translate(scale(fs.lBox, [1, back - front, top - bottom]), [
            left,
            front,
            bottom,
          ]);
        }
      } else {
        if (front === back) {
          return translate(scale(fs.fBox, [right - left, 1, top - bottom]), [
            left,
            front,
            bottom,
          ]);
        } else {
          return translate(
            scale(fs.box, [right - left, back - front, top - bottom]),
            [left, front, bottom]
          );
        }
      }
    }
  };

  return makeAbsolute(build());
};

export const Box = ([x, y, z], options = {}) => {
  const [computedC1, computedC2] = buildCorners(x, y, z);
  const { c1 = computedC1, c2 = computedC2 } = options;
  return makeBox(c1, c2);
};
