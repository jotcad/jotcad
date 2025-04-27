import { assets, withAssets } from './assets.js';
import { cgal, cgalIsReady } from './getCgal.js';
import { compose, identity, invert, matrix, rotateX, rotateY, rotateZ, scale, translate, transform } from './transform.js';

import { Point } from './point.js';

import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Simplify Rotations', (t) => {
  t.is('x 0.5', rotateX(0.5));
  t.is('y 0.5', rotateY(0.5));
  t.is('z 0.5', rotateZ(0.5));
  t.is('x 1/2', cgal.SimplifyTransform(rotateX(0.5)));
  t.is('y 1/2', cgal.SimplifyTransform(rotateY(0.5)));
  t.is('z 1/2', cgal.SimplifyTransform(rotateZ(0.5)));
});

test('Simplify Translate', (t) => {
  t.is('t 0.5 0.5 0.5', translate(0.5, 0.5, 0.5));
  t.is('t 1/2 1/2 1/2', cgal.SimplifyTransform(translate(0.5, 0.5, 0.5)));
});

test('Simplify Scale', (t) => {
  t.is('s 0.5 0.5 0.5', scale(0.5, 0.5, 0.5));
  t.is('s 1/2 1/2 1/2', cgal.SimplifyTransform(scale(0.5, 0.5, 0.5)));
});

test('Invert', (t) => {
  t.is('i', invert());
  t.is('x 3/4', cgal.SimplifyTransform(compose(invert(), rotateX('1/4'))));
});

test('Compose', (t) => {
  // The rotations cancel out back to the identity matrix.
  t.is(undefined, cgal.SimplifyTransform(compose(rotateX(0.5), rotateX(-0.5))));
  // These two compose to a rotation along the third axis.
  t.is('z 1/2', cgal.SimplifyTransform(compose(rotateX(0.5), rotateY(0.5))));
  // These produce a complex matrix.
  t.is('m 0 0 1 0 -1 0 1 0 0 0 0 0 ', cgal.SimplifyTransform(compose(rotateX(0.5), rotateY(0.25))));
});

test('Translate Point', (t) => {
  withAssets(() => {
    const transformedPoint = transform(translate(3, 2, 1), Point(1, 2, 3));
    const id = cgal.MakeAbsolute(assets, transformedPoint);
    t.is('v 4 4 4\np 0\n', assets.text[id]);
  });
});

test('Rotate Point', (t) => {
  withAssets(() => {
    t.is('v 0 -1 0\np 0\n', assets.text[cgal.MakeAbsolute(assets, transform(rotateZ(1/4), Point(1, 0, 0)))]);
    t.is('v -1 0 0\np 0\n', assets.text[cgal.MakeAbsolute(assets, transform(rotateZ(1/2), Point(1, 0, 0)))]);
    t.is('v -451/901 -780/901 0\np 0\n', assets.text[cgal.MakeAbsolute(assets, transform(rotateZ(1/3), Point(1, 0, 0)))]);
  });
});
