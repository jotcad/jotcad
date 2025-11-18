import { describe, it } from 'node:test';
import { fileURLToPath } from 'url';
import path, { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import { Orb } from './orb.js';
import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('Orb', () => {
  it('should create a 3d orb', async () => {
    await withTestAssets('should create a 3d orb', async (assets) => {
      const orb = Orb(assets, 1, 1, 1); // Example Orb with dimensions
      const image = await renderPng(assets, orb, {
        view: { position: [3 * 2, 4 * 2, 5 * 2] }, // Similar view to Box3 test
        width: 512,
        height: 512,
      });
      assert.ok(await testPng(assets, 'orb.test.png', image)); // Reference image name
    });
  });
});
