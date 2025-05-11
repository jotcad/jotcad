import { cgal, cgalIsReady } from './getCgal.js';

import { Box3 } from './box.js';
import { cut } from './cut.js';
import { renderPng } from './renderPng.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('corner', (t) =>
  withAssets(async (assets) => {
    const box = Box3(assets, [0, 2], [0, 2], [0, 2]);
    const tool = Box3(assets, [1, 2], [1, 2], [1, 2]);
    const cutBox = cut(assets, box, [tool]);
    const image = await renderPng(assets, cutBox, {
      view: { position: [15, 15, 15] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('cut.test.corner.png', image));
  }));
