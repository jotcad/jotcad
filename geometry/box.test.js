import { Box2, Box3 } from './box.js';
import { cgal, cgalIsReady } from './getCgal.js';

import { renderPng } from './renderPng.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Box3', (t) =>
  withAssets(async (assets) => {
    const box = Box3(assets, [-1, -1, -1], [1, 1, 1]);
    const image = await renderPng(assets, box, {
      view: { position: [3 * 2, 4 * 2, 5 * 2] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('box.test.Box3.png', image));
  }));

test('Box2', (t) =>
  withAssets(async (assets) => {
    const box = Box2(assets, [0, 0], [1, 1]);
    const image = await renderPng(assets, box, {
      view: { position: [3, 4, 5] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('box.test.Box2.png', image));
  }));
