import { cgal, cgalIsReady } from './getCgal.js';
import {
  compose,
  makeIdentity,
  makeInvert,
  makeMove,
  makeRotateX,
  makeRotateY,
  makeRotateZ,
  makeScale,
} from './transform.js';

import { Point } from './point.js';
import test from 'ava';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Simplify Rotations', (t) => {
  t.is('x 0.5 0.5', makeRotateX(0.5));
  t.is('y 0.5 0.5', makeRotateY(0.5));
  t.is('z 0.5 0.5', makeRotateZ(0.5));
  t.is('x 1/2 0.5', cgal.SimplifyTransform(makeRotateX(0.5)));
  t.is('y 1/2 0.5', cgal.SimplifyTransform(makeRotateY(0.5)));
  t.is('z 1/2 0.5', cgal.SimplifyTransform(makeRotateZ(0.5)));
});

test('Simplify Move', (t) => {
  t.is('t 0.5 0.5 0.5 0.5 0.5 0.5', makeMove(0.5, 0.5, 0.5));
  t.is(
    't 1/2 1/2 1/2 0.5 0.5 0.5',
    cgal.SimplifyTransform(makeMove(0.5, 0.5, 0.5))
  );
});

test('Simplify Scale', (t) => {
  t.is('s 0.5 0.5 0.5 0.5 0.5 0.5', makeScale(0.5, 0.5, 0.5));
  t.is(
    's 1/2 1/2 1/2 0.5 0.5 0.5',
    cgal.SimplifyTransform(makeScale(0.5, 0.5, 0.5))
  );
});

test('Invert', (t) => {
  t.is('i', makeInvert());
  t.is(
    'x 3/4 0.75',
    cgal.SimplifyTransform(compose(makeInvert(), makeRotateX('1/4')))
  );
});

test('Compose', (t) => {
  // The rotations cancel out back to the identity matrix.
  t.is(
    undefined,
    cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateX(-0.5)))
  );
  // These two compose to a rotation along the third axis.
  t.is(
    'z 1/2 0.5',
    cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateY(0.5)))
  );
  // These produce a complex matrix.
  t.is(
    'm 0 0 -1 0 -1 0 -1 0 0 0 0 0 0 0 -1 0 -1 0 -1 0 0 0 0 0',
    cgal.SimplifyTransform(compose(makeRotateX(0.5), makeRotateY(0.25)))
  );
});

test('Translate Point', (t) => {
  withAssets((assets) => {
    const transformedPoint = Point(assets, 1, 2, 3).move(3, 2, 1);
    const id = cgal.MakeAbsolute(assets, transformedPoint);
    t.is('v 4 4 4 4 4 4\np 0\n', assets.text[id]);
  });
});

test('Rotate Point', (t) => {
  withAssets((assets) => {
    t.is(
      'v 0 -1 0 0 -1 0\np 0\n',
      assets.text[
        cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 4))
      ]
    );
    t.is(
      'v -1 0 0 -1 0 0\np 0\n',
      assets.text[
        cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 2))
      ]
    );
    t.is(
      'v -451/901 -780/901 0 -0.500555 -0.865705 0\np 0\n',
      assets.text[
        cgal.MakeAbsolute(assets, Point(assets, 1, 0, 0).rotateZ(1 / 3))
      ]
    );
  });
});
