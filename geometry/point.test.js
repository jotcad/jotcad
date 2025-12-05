import { Point, Points } from './point.js';
import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { withTestAssets } from './test_session_util.js';

describe('point', () => {
  it('should create a point', async () => {
    await withTestAssets('should create a point', async (assets) => {
      assert.deepEqual(
        assets.getText(Point(assets, 1, 0, 0).geometry),
        'v 1 0 0 1 0 0\np 0\n'
      );
    });
  });

  it('should create points', async () => {
    await withTestAssets('should create points', async (assets) =>
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

  it('should round trip a point', async () => {
    await withTestAssets('should round trip a point', async (assets) => {
      const point = Point(assets, 10, 20, 30);
      const textRepresentation = assets.getText(point.geometry);
      assert.deepEqual(textRepresentation, 'v 10 20 30 10 20 30\np 0\n');
    });
  });
});
