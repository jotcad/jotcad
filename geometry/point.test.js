import { Point, Points } from './point.js';
import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { withAssets } from './assets.js';
import { getTestDir } from './test_util.js'; // Import getTestDir

describe('point', () => {
  it('should create a point', async () => {
    await withAssets(getTestDir('should create a point'), async (assets) => {
      assert.deepEqual(
        assets.getText(Point(assets, 1, 0, 0).geometry),
        'v 1 0 0 1 0 0\np 0'
      );
    });
  });

  it('should create points', async () => {
    await withAssets(getTestDir('should create points'), async (assets) =>
      assert.deepEqual(
        assets.getText(
          Points(assets, [
            [1, 0, 0],
            [2, 0, 0],
          ]).geometry
        ),
        'v 1 0 0 1 0 0\nv 2 0 0 2 0 0\np 0 1'
      )
    );
  });
});
