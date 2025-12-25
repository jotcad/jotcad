import { describe, it } from 'node:test';
import { withTestAssets } from './test_session_util.js';
import { renderPng } from './renderPng.js'; // Corrected import
import { testPng } from './test_png.js'; // Corrected import

import { Box3 } from './box.js'; // Assuming Box3 can be used as input
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

  // Add more test cases for different scenarios if needed
});
