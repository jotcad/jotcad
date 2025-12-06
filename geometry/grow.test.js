import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

import { Box3 } from './box.js';
import { Point } from './point.js';
import { grow } from './grow.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('grow', (t) =>
  it('should grow a point into a box', async () => {
    await withTestAssets('should grow a point into a box', async (assets) => {
      const point = Point(assets, 1, 1, 1);
      assert.deepEqual(assets.getText(point.geometry), 'v 1 1 1 1 1 1\np 0\n');
      const tool = Box3(assets, [0, 2], [0, 2], [0, 2]);
      const grown = grow(assets, point, [tool]);
      const textRepresentation = assets.getText(grown.geometry);
      assert.deepEqual(
        textRepresentation,
        `V 8
v 3 3 3 3 3 3
v 3 1 3 3 1 3
v 3 1 1 3 1 1
v 1 1 3 1 1 3
v 1 3 1 1 3 1
v 1 1 1 1 1 1
v 3 3 1 3 3 1
v 1 3 3 1 3 3
T 12
t 1 2 0
t 2 6 0
t 5 4 2
t 1 3 2
t 4 5 7
t 3 5 2
t 4 6 2
t 4 7 0
t 5 3 7
t 6 4 0
t 7 3 0
t 3 1 0
`
      );
      const image = await renderPng(assets, grown, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/grow.test.box.png`, image)
      );
    });
  }));
