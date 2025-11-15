import './transform.js';

import { beforeEach, describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { makeAbsolute } from './makeAbsolute.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';
import { getTestDir } from './test_util.js'; // Import getTestDir

describe('arc', () => {
  it('should build a full arc', async () => {
    await withAssets(getTestDir('should build a full arc'), async (assets) => {
      const full = await Arc2(assets, [-10, 10], [-10, 10], {
        start: 0,
        end: 1,
      });
      const absoluteFull = makeAbsolute(assets, full);
      const image = await renderPng(assets, full, {
        view: { position: [0, 0, 100] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.full.png', image));
    });
  });

  it('should build an arc with the specified give', async () => {
    await withAssets(getTestDir('should build an arc with the specified give'), async (assets) => {
      const full = await Arc2(assets, [-10, 10], [-10, 10], { give: 1 });
      const absoluteFull = makeAbsolute(assets, full);
      const image = await renderPng(assets, full, {
        view: { position: [0, 0, 100] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.give.png', image));
    });
  });

  it('should build a half-arc', async () => {
    await withAssets(getTestDir('should build a half-arc'), async (assets) => {
      const half = await Arc2(assets, [-10, 10], [-10, 10], {
        start: 0,
        end: '1/2',
      });
      const image = await renderPng(assets, half, {
        view: { position: [0, 0, 100] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.half.png', image));
    });
  });

  it('should build a quarter arc', async () => {
    await withAssets(getTestDir('should build a quarter arc'), async (assets) => {
      const half = await Arc2(assets, [-10, 10], [-10, 10], {
        start: 0,
        end: '1/4',
      });
      const image = await renderPng(assets, half, {
        view: { position: [0, 0, 10] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.quarter.png', image));
    });
  });

  it('should build a half square arc', async () => {
    await withAssets(getTestDir('should build a half square arc'), async (assets) => {
      const half = await Arc2(assets, [-1, 1], [-1, 1], {
        sides: 4,
        start: 0,
        end: '1/2',
      });
      const image = await renderPng(assets, half, {
        view: { position: [0, 0, 10] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.half_square.png', image));
    });
  });

  it('should build a quarter square arc', async () => {
    await withAssets(getTestDir('should build a quarter square arc'), async (assets) => {
      const quarter = await Arc2(assets, [-1, 1], [-1, 1], {
        sides: 4,
        start: 0,
        end: '1/4',
      });
      const image = await renderPng(assets, quarter, {
        view: { position: [0, 0, 10] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.quarter_square.png', image));
    });
  });

  it('should build a half square arc with the flats rotated', async () => {
    await withAssets(getTestDir('should build a half square arc with the flats rotated'), async (assets) => {
      const quarter = await Arc2(assets, [-1, 1], [-1, 1], {
        sides: 4,
        start: 0,
        end: '2/4',
        spin: '1/8',
      });
      const image = await renderPng(assets, quarter, {
        view: { position: [0, 0, 10] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('arc.test.half_square_spin.png', image));
    });
  });
});
