import { Point, Points } from './point.js';
import { assets, withAssets } from './assets.js';

import { cgalIsReady } from './getCgal.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});


test('Point', (t) => {
  withAssets(() => {
    t.deepEqual(assets.text[Point(1, 0, 0).geometry], 'v 1 0 0\np 0');
  });
});

test('Points', (t) => {
  withAssets(() => {
    t.deepEqual(assets.text[Points([[1, 0, 0], [2, 0 ,0]]).geometry], 'v 1 0 0\nv 2 0 0\np 0 1');
  });
});
