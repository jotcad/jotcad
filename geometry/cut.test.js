import { describe, it } from 'node:test';
import { fileURLToPath } from 'url';
import path, { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { cut } from './cut.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withTestAssets('should cut the corner out of a box', async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 2]);
      const cutBox = cut(assets, box, [tool]);
      const image = await renderPng(assets, cutBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng(assets, 'cut.test.corner.png', image));
    });
  }));

describe('masked cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withTestAssets('should cut the corner out of a box (masked)', async (assets) => {
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
      assert.ok(await testPng(assets, 'cut.test.masked_cut.png', image));
    });
  }));
