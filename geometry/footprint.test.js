import { describe, it } from 'node:test';
import { strict as assert } from 'node:assert';
import { withTestAssets } from './test_session_util.js';
import { Box3 } from './box.js';
import { cut } from './cut.js';
import { footprint } from './footprint.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';

describe('footprint', () => {
  it('should create a 2D footprint from a 3D box', async () => {
    await withTestAssets('footprint of a 3d box', async (assets) => {
      const box = Box3(assets, [0, 10], [0, 10], [0, 10]);
      const footprintShape = footprint(assets, box);

      assert.ok(
        footprintShape.geometry !== undefined &&
          footprintShape.geometry !== null,
        'Footprint should return a valid geometry ID'
      );
      // Test the visual output using renderPng and testPng
      const outputPath = assets.pathTo('footprint_box.png');
      const buffer = await renderPng(footprintShape, outputPath, {
        width: 100,
        height: 100,
      });
      await testPng(outputPath, buffer);
    });
  });

  it('should create a 2D footprint from an object with a hole', async () => {
    await withTestAssets(
      'footprint of an object with a hole',
      async (assets) => {
        const largeBox = Box3(assets, [-10, 10], [-10, 10], [0, 1]);
        const smallBox = Box3(assets, [-5, 5], [-5, 5], [0, 1]);
        const hollowSquareShape = cut(assets, largeBox, [smallBox]);
        const footprintShapeWithHole = footprint(assets, hollowSquareShape);

        assert.ok(
          footprintShapeWithHole.geometry !== undefined &&
            footprintShapeWithHole.geometry !== null,
          'Footprint with hole should return a valid geometry ID'
        );

        const outputPath = assets.pathTo('footprint_hollow_square.png');
        const buffer = await renderPng(footprintShapeWithHole, outputPath, {
          width: 100,
          height: 100,
        });
        await testPng(outputPath, buffer);
      }
    );
  });
});
