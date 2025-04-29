import { cgal, cgalIsReady } from './getCgal.js';

import { Point } from './point.js';
import { shape } from './shape.js';
import { assets, withAssets } from './assets.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Fill', (t) => {
  withAssets(() => {
    const box = shape({ geometry: cgal.Link(assets, [Point(1, 0, 0), Point(0, 1, 0), Point(0, 0, 1)], true, false) });
    const id = cgal.Fill(assets, [box], false, false);
    t.deepEqual(assets.text[id], 'v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n');
  });
});
