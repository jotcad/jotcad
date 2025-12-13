import { Box2, Box3 } from './box.js';
import { describe, it } from 'node:test';

import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';
import { writeFile } from 'node:fs/promises';

describe('box', () => {
  it('should created a 3d box', async () => {
    await withTestAssets('should created a 3d box', async (assets) => {
      const box = await Box3(assets, [-1, 1], [-1, 1], [-1, 1]);
      const image = await renderPng(assets, box, {
        view: { position: [3 * 2, 4 * 2, 5 * 2] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/box.test.Box3.png`, image)
      );
    });
  });

  it('should create a 2d box', async () => {
    await withTestAssets('should create a 2d box', async (assets) => {
      const box = await Box2(assets, [0, 1], [0, 1]);
      const image = await renderPng(assets, box, {
        view: { position: [3, 4, 5] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/box.test.Box2.png`, image)
      );
    });
  });

  it('should produce a well-structured 3d box', async () => {
    await withTestAssets(
      'should produce a well-structured 3d box',
      async (assets) => {
        const box = await Box3(assets, [-1, 1], [-1, 1], [-1, 1]);
        assert.ok(
          cgal.Test(assets, box, {
            doesBoundAVolume: true,
            doesNotSelfIntersect: true,
            isClosed: true,
          }),
          'Box3 should be a closed, non-self-intersecting volume'
        );
      }
    );
  });
});
