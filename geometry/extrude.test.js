import { describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { extrude } from './extrude.js';
import { fill2 } from './fill.js';
import { makeShape } from './shape.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withAssets } from './assets.js';

describe('extrude', () => {
  it('should extrude a triangle', () =>
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
      const filledTriangle = fill2(assets, [triangle], true);
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
      assert.ok(await testPng('extrude.test.triangle.png', image));
    }));

  it('should extrude a triangle while shrinking the top', () =>
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
      const filledTriangle = fill2(assets, [triangle], true);
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
      assert.ok(await testPng('extrude.test.shrink.png', image));
    }));
});
