import { describe, it } from 'node:test';
import { withTestAssets } from './test_session_util.js';
import { renderPng } from './renderPng.js'; // Corrected import
import { testPng } from './test_png.js'; // Corrected import

import { Arc2 } from './arc.js';
import { Box3 } from './box.js'; // Assuming Box3 can be used as input
import { cgal } from './getCgal.js';
import { fill2 } from './fill.js';
import { makeShape } from './shape.js';
import { Point } from './point.js';
import { Wrap3 } from './wrap.js';
import assert from 'node:assert/strict';

describe('Wrap3', () => {
  it('should create a 3D alpha wrap around a box', async () => {
    await withTestAssets('wrap3_box_test', async (assets) => {
      // Create an input shape (e.g., a simple Box3)
      const inputShape = await Box3(assets, [-5, 5], [-5, 5], [-5, 5]);

      // Apply Wrap3
      // Choose alpha and offset values for a noticeable wrap
      const alpha = 5.0; // Adjust based on expected feature size
      const offset = 1.0; // Distance from input
      const wrappedShape = await Wrap3(assets, inputShape, alpha, offset);

      // Render the wrapped shape to a PNG and get the buffer
      const imageBuffer = await renderPng(assets, wrappedShape, {
        view: { position: [15, 15, 15] }, // Adjust view as needed
        width: 512,
        height: 512,
        // renderPng does not take a 'path' argument directly for saving
      });

      // Assert that the generated PNG matches the reference PNG.
      // A reference PNG (geometry.wrap.test.box.png) will need to be created manually
      // after the first successful run with expected output.
      assert.ok(
        await testPng(
          `${import.meta.dirname}/geometry.wrap.test.box.png`, // Absolute path for expected
          imageBuffer // Pass the buffer directly
        ),
        'Generated PNG for Wrap3 around a box should match the reference.'
      );
    });
  });

  it('should create a 3D alpha wrap around a filled disk', async () => {
    await withTestAssets('wrap3_disk_test', async (assets) => {
      // Create a filled disk (which uses faces_)
      const disk = fill2(assets, [Arc2(assets, [-10, 10], [-10, 10])], true);

      // Apply Wrap3
      const alpha = 0.5;
      const offset = 0.5;
      const wrappedShape = await Wrap3(assets, disk, alpha, offset);

      // Render the wrapped shape
      const imageBuffer = await renderPng(assets, wrappedShape, {
        view: { position: [20, 20, 60] },
        width: 512,
        height: 512,
      });

      assert.ok(
        await testPng(
          `${import.meta.dirname}/geometry.wrap.test.disk.png`,
          imageBuffer
        ),
        'Generated PNG for Wrap3 around a disk should match the reference.'
      );
    });
  });

  it('should create a 3D alpha wrap around a square outline (segments)', async () => {
    await withTestAssets('wrap3_segments_test', async (assets) => {
      // Create a square outline (segments)
      const square = makeShape({
        geometry: cgal.Link(
          assets,
          [
            await Point(assets, -10, -10, 0),
            await Point(assets, 10, -10, 0),
            await Point(assets, 10, 10, 0),
            await Point(assets, -10, 10, 0),
          ],
          true,
          false
        ),
      });

      // Apply Wrap3
      const alpha = 5.0;
      const offset = 2.0;
      const wrappedShape = await Wrap3(assets, square, alpha, offset);

      // Render the wrapped shape
      const imageBuffer = await renderPng(assets, wrappedShape, {
        view: { position: [20, 20, 20] },
        width: 512,
        height: 512,
      });

      assert.ok(
        await testPng(
          `${import.meta.dirname}/geometry.wrap.test.segments.png`,
          imageBuffer
        ),
        'Generated PNG for Wrap3 around segments should match the reference.'
      );
    });
  });
});
