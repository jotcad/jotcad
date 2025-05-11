import { cgal, cgalIsReady } from './getCgal.js';

import { Box3 } from './box.js';
import { join } from './join.js';
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
    const tool = Box3(assets, [1, 2], [1, 2], [1, 4]);
    const joinedBox = join(assets, box, [tool]);
    const image = await renderPng(assets, joinedBox, {
      view: { position: [15, 15, 15] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('join.test.corner.png', image));
  }));
