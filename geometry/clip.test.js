import { describe, it } from 'node:test';
import { fileURLToPath } from 'url';
import path, { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { clip } from './clip.js';
import { getTestDir } from './test_util.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('clip', () =>
  it('should clip the corner of a box', async () => {
    await withAssets(getTestDir('should clip the corner of a box'), async (assets) => {
      const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
      const tool = await Box3(assets, [1, 2], [1, 2], [1, 4]);
      const joinedBox = clip(assets, box, [tool]);
      const image = await renderPng(assets, joinedBox, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng(path.join(__dirname, 'clip.test.corner.png'), image));
    });
  }));
