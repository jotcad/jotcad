import { cgal, cgalIsReady } from './getCgal.js';

import { renderPng } from './renderPng.js';

import { Point } from './point.js';
import { shape } from './shape.js';
import test from 'ava';
import { writeFile } from 'node:fs/promises';
import { withAssets } from './assets.js';
import { testPng } from './test_png.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test("triangle", (t) =>
  withAssets(async (assets) => {
    const id = cgal.Link(assets, [Point(assets, 1, 0, 0), Point(assets, 0, 1, 0), Point(assets, 0, 0, 1)], true, true);
    t.deepEqual(assets.text[id], 'v 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 2 2 1 1 0\n');
    const triangle = shape({ geometry: id });
    const image = await renderPng(assets, triangle, { width: 512, height: 512 });
    t.true(await testPng('renderPng.test.triangle.png', image));
  }));
