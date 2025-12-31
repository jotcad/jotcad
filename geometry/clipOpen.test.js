import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import { Point } from './point.js';
import { setTag } from './tag.js';
import './transform.js';
import assert from 'node:assert/strict';
import { clipOpen } from './clipOpen.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('clipOpen', () => {
  it('should open clip a box with an infinite plane', async () => {
    await withTestAssets(
      'should open clip a box with an infinite plane',
      async (assets) => {
        const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
        // Infinite plane at Z=1 (negative side is Z < 1)
        const tool = setTag(
          Point(assets, 0, 0, 0).move(0, 0, 1),
          'isPlane',
          true
        );
        const clippedBox = clipOpen(assets, box, [tool]);
        const image = await renderPng(assets, clippedBox, {
          view: { position: [15, 15, 15] },
          width: 512,
          height: 512,
        });
        assert.ok(
          await testPng(`${import.meta.dirname}/clipOpen.test.plane.png`, image)
        );
      }
    );
  });
});
