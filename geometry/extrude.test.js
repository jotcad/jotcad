import { cgal, cgalIsReady } from './getCgal.js';

import { Arc2 } from './arc.js';
import { Point } from './point.js';
import { extrude } from './extrude.js';
import { fill } from './fill.js';
import { makeShape } from './shape.js';
import { renderPng } from './renderPng.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('triangle', (t) =>
  withAssets(async (assets) => {
    const triangle = makeShape({
      geometry: cgal.Link(
        assets,
        [
          await Point(assets, 1, 0, 0),
          await Point(assets, 0, 1, 0),
          await Point(assets, 0, 0, 1),
        ],
        true,
        false
      ),
    });
    const filledTriangle = fill(assets, [triangle], true);
    const extrudedTriangle = extrude(
      assets,
      filledTriangle,
      (await Point(assets, 0, 0, 0)).move(0, 0, 1),
      (await Point(assets, 0, 0, 0)).move(0, 0, -1)
    );
    const image = await renderPng(assets, extrudedTriangle, {
      view: { position: [5, 5, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('extrude.test.triangle.png', image));
  }));

test('shrink', (t) =>
  withAssets(async (assets) => {
    const triangle = makeShape({
      geometry: cgal.Link(
        assets,
        [
          await Point(assets, 1, 0, 0),
          await Point(assets, 0, 1, 0),
          await Point(assets, 0, 0, 1),
        ],
        true,
        false
      ),
    });
    const filledTriangle = fill(assets, [triangle], true);
    const extrudedTriangle = extrude(
      assets,
      filledTriangle,
      (await Point(assets, 0, 0, 0)).scale('1/2', '1/2').move(0, 0, 1),
      (await Point(assets, 0, 0, 0)).move(0, 0, -1)
    );
    const image = await renderPng(assets, extrudedTriangle, {
      view: { position: [5, 5, 10] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('extrude.test.shrink.png', image));
  }));
