import { describe, it } from 'node:test';

import { Face } from './face.js';
import assert from 'node:assert/strict';
import { withAssets } from './assets.js';
import { getTestDir } from './test_util.js'; // Import getTestDir

describe('face', () => {
  it('should create a face', async () => {
    await withAssets(getTestDir('should create a face'), async (assets) => {
      assert.deepEqual(
        assets.getText(
          Face(assets, [
            [1, 0, 0],
            [1, 1, 0],
            [0, 1, 0],
            [0, 0, 0],
          ]).geometry
        ),
        'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\n'
      );
    });
  });

  it('should create a face with a hole', async () => {
    await withAssets(getTestDir('should create a face with a hole'), async (assets) => {
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
      assert.deepEqual(
        assets.getText(fwh.geometry),
        'v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 0 1 2 3\nv 0.75 0.25 0.25\nv 0.75 0.75 0.25\nv 0.25 0.75 0.25\nv 0.25 0.25 0.25\nh 0 1 2 3\n'
      );
    });
  });
});
