import { describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { fill2 } from './fill.js';
import { makeShape } from './shape.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('fill', () => {
  it('should fill a triangle', async () => {
    await withAssets(async (assets) => {
      const box = makeShape({
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
      const filledBox = fill2(assets, [box], false);
      assert.deepEqual(
        assets.getText(filledBox.geometry),
        'V 3\nv 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n'
      );
      const image = await renderPng(assets, filledBox, {
        view: { position: [0, 0, 20] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('fill.test.triangle.png', image));
    });
  });

  it('should fill the perimeter of a hollow ring', async () => {
    await withAssets(async (assets) => {
      const outline = [
        Arc2(assets, [-20, 20], [-20, 20]),
        Arc2(assets, [-10, 10], [-10, 10]),
      ];
      const outlineImage = await renderPng(
        assets,
        makeShape({ shapes: outline }),
        {
          view: { position: [0, 0, 100] },
          width: 512,
          height: 512,
        }
      );
      assert.ok(await testPng('fill.test.ring_outline.png', outlineImage));
      const ring = fill2(assets, outline, true);
      const image = await renderPng(assets, ring, {
        view: { position: [0, 0, 100] },
        width: 512,
        height: 512,
      });
      assert.ok(await testPng('fill.test.ring.png', image));
    });
  });
});
