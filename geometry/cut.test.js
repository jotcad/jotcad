import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { Box3 } from './box.js';
import { cut } from './cut.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('cut', (t) =>
  it('should cut the corner out of a box', () =>
    withAssets(async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 2]);
      const cutBox = cut(assets, box, [tool]);
      const image = await renderPng(assets, cutBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('cut.test.corner.png', image));
    })));
