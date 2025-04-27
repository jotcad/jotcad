import { Face } from './face.js';
import { assets, withAssets } from './assets.js';

import { cgalIsReady } from './getCgal.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Face', (t) => {
  withAssets(() => {
    t.deepEqual(assets.text[Face([[1, 0, 0], [1, 1, 0], [0, 1, 0], [0, 0, 0]]).geometry], 'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\n');
  });
});

test('FaceWithHole', (t) => {
  withAssets(() => {
    const fwh = Face(
      [[1, 0, 0], [1, 1, 0], [0, 1, 0], [0, 0, 0]],
      [[[0.75, 0.25, 0.25], [0.75, 0.75, 0.25], [0.25, 0.75, 0.25], [0.25, 0.25, 0.25]]]);
    t.deepEqual(assets.text[fwh.geometry], 'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\nv 0.75 0.25 0.25\nv 0.75 0.75 0.25\nv 0.25 0.75 0.25\nv 0.25 0.25 0.25\nh 0 1 2 3\n')
  });
});
