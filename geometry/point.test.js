import { Point, Points } from './point.js';

import { cgalIsReady } from './getCgal.js';
import test from 'ava';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});


test('Point', (t) => {
  withAssets((assets) => {
    t.deepEqual(assets.text[Point(assets, 1, 0, 0).geometry], 'v 1 0 0 1 0 0\np 0');
  });
});

test('Points', (t) => {
  withAssets((assets) => {
    t.deepEqual(assets.text[Points(assets, [[1, 0, 0], [2, 0 ,0]]).geometry], 'v 1 0 0 1 0 0\nv 2 0 0 2 0 0\np 0 1');
  });
});
