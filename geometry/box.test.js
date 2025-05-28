import { Box2, Box3 } from './box.js';
import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

describe('box', () => {
  it('should created a 3d box', () =>
    withAssets(async (assets) => {
      const box = await Box3(assets, [-1, 1], [-1, 1], [-1, 1]);
      const image = await renderPng(assets, box, {
        view: { position: [3 * 2, 4 * 2, 5 * 2] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('box.test.Box3.png', image));
    }));

  it('should create a 2d box', () =>
    withAssets(async (assets) => {
      const box = await Box2(assets, [0, 1], [0, 1]);
      const image = await renderPng(assets, box, {
        view: { position: [3, 4, 5] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('box.test.Box2.png', image));
    }));
});
