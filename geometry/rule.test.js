import assert from 'node:assert/strict';
import { describe, it } from 'node:test';
import { withTestAssets } from './test_session_util.js';
import { Points } from './point.js'; // Import Points
import { rule } from './rule.js'; // The function we are testing
import { renderPng } from './renderPng.js'; // Import renderPng
import { testPng } from './test_png.js'; // Import testPng

const __dirname = import.meta.dirname; // Modern ES Module __dirname

describe('rule', () => {
  it('should generate a ruled surface between two simple polygonal chains', async () => {
    await withTestAssets('rule a simple surface', async (assets) => {
      // Create two simple polygonal chains (squares for example)
      // from_shape: A square in the XY plane
      const from_shape = Points(assets, [
        [0, 0, 0],
        [10, 0, 0],
        [10, 10, 0],
        [0, 10, 0],
        [0, 0, 0], // Close the loop
      ]);

      // to_shape: A translated and scaled square in a parallel plane
      const to_shape = Points(assets, [
        [0, 0, 10],
        [5, 0, 10],
        [5, 5, 10],
        [0, 5, 10],
        [0, 0, 10], // Close the loop
      ]);

      // Call the rule operation
      const ruled_surface = rule(assets, [from_shape, to_shape]);

      // Assert that a GeometryId is returned (indicating a mesh was generated)
      assert.ok(
        ruled_surface.geometry,
        'Expected a geometryId for the ruled surface'
      );

      // Render the generated surface to a PNG
      const image = await renderPng(assets, ruled_surface, {
        view: { position: [20, 20, 20], target: [0, 0, 5] }, // Adjust view as needed
        width: 512,
        height: 512,
      });

      // Compare the generated PNG with a reference image
      assert.ok(
        await testPng(`${import.meta.dirname}/rule.test.png`, image),
        'Generated image does not match reference'
      );
    });
  });

  // Additional test cases can be added here for different scenarios
  // For example:
  // - Chains with different number of points (RuleLoopsSA expects same size if not using seam search)
  // - More complex shapes
  // - Testing with different options (seed, stopping rules)
});
