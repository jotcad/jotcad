import { cgal, cgalIsReady } from './getCgal.js';

import { Point } from './point.js';
import { makeShape } from './shape.js';
import { renderPng } from './renderPng.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('triangle', (t) =>
  withAssets(async (assets) => {
    const id = cgal.Link(
      assets,
      [Point(assets, 1, 0, 0), Point(assets, 0, 1, 0), Point(assets, 0, 0, 1)],
      true,
      true
    );
    t.deepEqual(
      assets.getText(id),
      'V 3\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 2 2 1 1 0\n'
    );
    const triangle = makeShape({ geometry: id });
    const image = await renderPng(assets, triangle, {
      width: 512,
      height: 512,
    });
    t.true(await testPng('renderPng.test.triangle.png', image));
  }));
