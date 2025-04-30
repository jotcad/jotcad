import { assets, withAssets } from './assets.js';
import { cgal, cgalIsReady } from './getCgal.js';

import { Box } from './box.js';
import { renderPng } from './renderPng.js';
import { shape } from './shape.js';
import test from 'ava';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('Box', (t) =>
  withAssets(async () => {
    const box = Box([0, 0, 0], [1, 1, 1]);
    const image = await renderPng(assets, box, { view: { position: [3, 4, 5] }, width: 512, height: 512 });
    await writeFile('box.test.png', Buffer.from(image));
  }));
