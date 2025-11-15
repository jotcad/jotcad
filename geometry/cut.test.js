import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { cut } from './cut.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { getTestDir } from './test_util.js'; // Import getTestDir

describe('cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withAssets(getTestDir('should cut the corner out of a box'), async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 2]);
      const cutBox = cut(assets, box, [tool]);
      const image = await renderPng(assets, cutBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('cut.test.corner.png', image));
    });
  }));

describe('masked cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withAssets(getTestDir('should cut the corner out of a box (masked)'), async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 2]);
      const mask = await Box3(assets, [0, 2], [1, 2], [1, 2]);
      const maskedTool = tool.derive({ mask });
      const cutBox = cut(assets, box, [maskedTool]);
      const image = await renderPng(assets, cutBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('cut.test.masked_cut.png', image));
    });
  }));
