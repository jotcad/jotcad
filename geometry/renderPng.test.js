import { cgal, cgalIsReady } from './getCgal.js';

import { renderPng } from './renderPng.js';
import { assets, withAssets } from './assets.js';

import { Point } from './point.js';
import { shape } from './shape.js';
import test from 'ava';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test("Render triangle", (t) =>
  withAssets(async () => {
    const id = cgal.Link(assets, [Point(1, 0, 0), Point(0, 1, 0), Point(0, 0, 1)], true, true);
    t.deepEqual(assets.text[id], 'v 1 0 0\nv 0 1 0\nv 0 0 1\ns 0 2 2 1 1 0\n');
    const triangle = shape({ geometry: id });
    const image = await renderPng(assets, triangle, { width: 512, height: 512 });
    await writeFile('renderPng.test.png', Buffer.from(image));
    t.is(image, 'xxx');
  }));
