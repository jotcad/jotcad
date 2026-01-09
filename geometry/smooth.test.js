import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import { Link } from './link.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { smooth } from './smooth.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';
import { writeFileSync } from 'node:fs';

describe('smooth', () => {
  it('should smooth a box edge', async () => {
    await withTestAssets('should smooth a box edge', async (assets) => {
      // Create a box 30x30x30
      const box = Box3(assets, [0, 30], [0, 30], [0, 30]);

      // Create a polyline along the edge (0,0,0) to (0,0,30)
      const line = Link(assets, [
        Point(assets, 0, 0, -1),
        Point(assets, 0, 0, 31),
      ]);

      // Apply smooth with the robust parameters found during testing.
      const smoothedBox = smooth(
        assets,
        box,
        [line],
        10, // radius
        15, // angleThreshold
        4, // resolution
        false, // skipFairing
        false, // skipRefine
        1 // fairingContinuity
      );

      // Check that geometry changed (simple vertex count check via text representation)
      const originalText = assets.getText(box.geometry);
      const smoothedText = assets.getText(smoothedBox.geometry);

      assert.notEqual(
        originalText,
        smoothedText,
        'Geometry should have changed'
      );

      // Write to .jot file for inspection
      writeFileSync(
        `${import.meta.dirname}/observed.smooth.test.edge.jot`,
        smoothedText
      );

      // Render for visual inspection with wireframe overlay
      const image = await renderPng(assets, smoothedBox, {
        view: { position: [-100, -100, 100], target: [15, 15, 15] },
        width: 512,
        height: 512,
        doWireframe: true,
      });

      assert.ok(
        await testPng(`${import.meta.dirname}/smooth.test.edge.png`, image)
      );
    });
  });
});
