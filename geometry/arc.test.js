import './transform.js';

import { cgal, cgalIsReady } from './getCgal.js';

import { Arc2 } from './arc.js';
import { makeAbsolute } from './makeAbsolute.js';
import { renderPng } from './renderPng.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('full', (t) =>
  withAssets(async (assets) => {
    const full = Arc2(
      assets,
      [-10, -10, -10],
      [10, 10, 10],
      undefined,
      0,
      1,
      0
    );
    const absoluteFull = makeAbsolute(assets, full);
    const image = await renderPng(assets, full, {
      view: { position: [0, 0, 100] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.full.png', image));
  }));

test('half', (t) =>
  withAssets(async (assets) => {
    const half = Arc2(
      assets,
      [-10, -10, -10],
      [10, 10, 10],
      undefined,
      0,
      '1/2',
      0
    );
    const image = await renderPng(assets, half, {
      view: { position: [0, 0, 100] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.half.png', image));
  }));

test('quarter', (t) =>
  withAssets(async (assets) => {
    const half = Arc2(
      assets,
      [-10, -10, -10],
      [10, 10, 10],
      undefined,
      0,
      '1/4',
      0
    );
    const image = await renderPng(assets, half, {
      view: { position: [0, 0, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.quarter.png', image));
  }));

test('half_square', (t) =>
  withAssets(async (assets) => {
    const half = Arc2(assets, [-1, -1, -1], [1, 1, 1], 4, 0, '1/2', 0);
    const image = await renderPng(assets, half, {
      view: { position: [0, 0, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.half_square.png', image));
  }));

test('quarter_square', (t) =>
  withAssets(async (assets) => {
    const quarter = Arc2(assets, [-1, -1, -1], [1, 1, 1], 4, 0, '1/4', 0);
    const image = await renderPng(assets, quarter, {
      view: { position: [0, 0, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.quarter_square.png', image));
  }));

test('half_square_spin', (t) =>
  withAssets(async (assets) => {
    const quarter = Arc2(assets, [-1, -1, -1], [1, 1, 1], 4, 0, '2/4', '1/8');
    const image = await renderPng(assets, quarter, {
      view: { position: [0, 0, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('arc.test.half_square_spin.png', image));
  }));
