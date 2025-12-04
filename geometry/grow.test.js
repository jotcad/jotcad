import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

import { Box2 } from './box.js';
import { Point } from './point.js';
import { grow } from './grow.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('grow', (t) =>
  it('should grow a point into a box', async () => {
    await withTestAssets('should grow a point into a box', async (assets) => {
      const point = Point(assets, [5, 5, 5]);
      const tool = Box2(assets, [0, 2], [0, 2]);
      const grown = grow(assets, point, [tool]);
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
