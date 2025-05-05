import { cgal, cgalIsReady } from './getCgal.js';

import { Arc2 } from './arc.js';
import { Point } from './point.js';
import { fill } from './fill.js';
import { renderPng } from './renderPng.js';
import { shape } from './shape.js';
import test from 'ava';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('triangle', (t) =>
  withAssets(async (assets) => {
    const box = shape({
      geometry: cgal.Link(
        assets,
        [
          Point(assets, 1, 0, 0),
          Point(assets, 0, 1, 0),
          Point(assets, 0, 0, 1),
        ],
        true,
        false
      ),
    });
    const filledBox = fill(assets, [box], false);
    t.deepEqual(
      assets.text[filledBox.geometry],
      'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n'
    );
    const image = await renderPng(assets, filledBox, {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('fill.test.triangle.png', image));
  }));

test('ring', (t) =>
  withAssets(async (assets) => {
    const outline = [
      Arc2(assets, [0, 0], [3, 3]),
      Arc2(assets, [1, 1], [2, 2]),
    ];
    const outlineImage = await renderPng(assets, shape({ shapes: outline }), {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('fill.test.ring_outline.png', outlineImage));
    const ring = fill(assets, outline, true);
    const image = await renderPng(assets, ring, {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    t.true(await testPng('fill.test.ring.png', image));
  }));
