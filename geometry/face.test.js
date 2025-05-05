import { Face } from './face.js';

import { cgalIsReady } from './getCgal.js';
import test from 'ava';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Face', (t) => {
  withAssets((assets) => {
    t.deepEqual(
      assets.text[
        Face(assets, [
          [1, 0, 0],
          [1, 1, 0],
          [0, 1, 0],
          [0, 0, 0],
        ]).geometry
      ],
      'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\n'
    );
  });
});

test('FaceWithHole', (t) => {
  withAssets((assets) => {
    const fwh = Face(
      assets,
      [
        [1, 0, 0],
        [1, 1, 0],
        [0, 1, 0],
        [0, 0, 0],
      ],
      [
        [
          [0.75, 0.25, 0.25],
          [0.75, 0.75, 0.25],
          [0.25, 0.75, 0.25],
          [0.25, 0.25, 0.25],
        ],
      ]
    );
    t.deepEqual(
      assets.text[fwh.geometry],
      'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\nv 0.75 0.25 0.25\nv 0.75 0.75 0.25\nv 0.25 0.75 0.25\nv 0.25 0.25 0.25\nh 0 1 2 3\n'
    );
  });
});
