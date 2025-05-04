import { assets, withAssets } from './assets.js';
import { cgal, cgalIsReady } from './getCgal.js';
import { translate, transform } from './transform.js';

import { Arc2 } from './arc.js';
import { testPng } from './test_png.js';
import { renderPng } from './renderPng.js';
import { shape } from './shape.js';
import test from 'ava';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('full', (t) =>
  withAssets(async () => {
    const full = Arc2([-1, -1, -1], [1, 1, 1], 64, 1, 1, 0);
    const image = await renderPng(assets, full, { view: { position: [0, 0, 20] }, width: 512, height: 512 });
    console.log(assets.text[full.geometry]);
    t.true(await testPng('arc.test.full.png', image));
  }));

test('half', (t) =>
  withAssets(async () => {
    const half = Arc2([-1, -1, -1], [1, 1, 1], 64, 0, '1/2', 0);
    const image = await renderPng(assets, half, { view: { position: [0, 0, 20] }, width: 512, height: 512 });
    t.true(await testPng('arc.test.half.png', image));
  }));

test('half_square', (t) =>
  withAssets(async () => {
    const half = Arc2([-1, -1, -1], [1, 1, 1], 4, 0, '1/2', 0);
    const image = await renderPng(assets, half, { view: { position: [0, 0, 20] }, width: 512, height: 512 });
    t.true(await testPng('arc.test.half_square.png', image));
  }));

test('quarter_square', (t) =>
  withAssets(async () => {
    const quarter = Arc2([-1, -1, -1], [1, 1, 1], 4, 0, '1/4', 0);
    const image = await renderPng(assets, quarter, { view: { position: [0, 0, 20] }, width: 512, height: 512 });
    t.true(await testPng('arc.test.quarter_square.png', image));
  }));

test('half_square_spin', (t) =>
  withAssets(async () => {
    const quarter = Arc2([-1, -1, -1], [1, 1, 1], 4, 0, '2/4', '1/8');
    const image = await renderPng(assets, quarter, { view: { position: [0, 0, 20] }, width: 512, height: 512 });
    t.true(await testPng('arc.test.half_square_spin.png', image));
  }));
