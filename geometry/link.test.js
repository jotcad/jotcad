import { cgal, cgalIsReady } from './getCgal.js';

import { Point } from './point.js';
import { shape } from './shape.js';
import { assets, withAssets } from './assets.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Link open', (t) => {
  withAssets(() => {
    const id = cgal.Link(assets, [Point(1, 0, 0), Point(0, 1, 0), Point(0, 0, 1)], false, false);
    t.deepEqual(assets.text[id], 'v 1 0 0\nv 0 1 0\nv 0 0 1\ns 0 1 1 2\n');
  });
});

test('Link closed', (t) => {
  withAssets(() => {
    const id = cgal.Link(assets, [Point(1, 0, 0), Point(0, 1, 0), Point(0, 0, 1)], true, false);
    t.deepEqual(assets.text[id], 'v 1 0 0\nv 0 1 0\nv 0 0 1\ns 0 1 1 2 2 0\n');

  });
});

test('Link reverse', (t) => {
  withAssets(() => {
    const id = cgal.Link(assets, [Point(1, 0, 0), Point(0, 1, 0), Point(0, 0, 1)], true, true);
    t.deepEqual(assets.text[id], 'v 1 0 0\nv 0 1 0\nv 0 0 1\ns 0 2 2 1 1 0\n');
  });
});
