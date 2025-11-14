import { describe, it } from 'node:test';

import { Orb } from './orb.js';
import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('Orb', () => {
  it('should create a 3d orb', async () => {
    await withAssets(async (assets) => {
      const orb = Orb(assets, 1, 1, 1); // Example Orb with dimensions
      const image = await renderPng(assets, orb, {
        view: { position: [3 * 2, 4 * 2, 5 * 2] }, // Similar view to Box3 test
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('orb.test.png', image)); // Reference image name
    });
  });
});
