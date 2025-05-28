import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { clip } from './clip.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('clip', () =>
  it('should clip the corner of a box', () =>
    withAssets(async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 4]);
      const joinedBox = clip(assets, box, [tool]);
      const image = await renderPng(assets, joinedBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('clip.test.corner.png', image));
    })));
