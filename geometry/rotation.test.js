import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { Point } from './point.js';
import './transform.js';
import { cgal } from './getCgal.js';
import { withTestAssets } from './test_session_util.js';

/**
 * JotCAD Rotation Expectations
 * ===========================
 *
 * Convention: Right-Handed Coordinate System (RHS)
 * Units: Turns (0.0 to 1.0, where 1.0 = 360 degrees)
 * Direction: Positive rotation is Counter-Clockwise (CCW) when looking from
 * the positive axis towards the origin.
 *
 * Reasoning:
 * ----------
 * 1. Right-Hand Rule (RHR): If your right thumb points along the positive axis
 *    of rotation, your fingers curl in the direction of positive rotation.
 * 2. Cross-Product Consistency: Rotations are linked to cross products in RHS.
 *    Axis A rotated 90 degrees around Axis B should result in Axis C if B x A = C.
 *    - Around X: X x Y = Z  => Y rotates to Z.
 *    - Around Y: Y x Z = X  => Z rotates to X.
 *    - Around Z: Z x X = Y  => X rotates to Y.
 * 3. 2D Compatibility: In the XY plane (rotation around Z), a positive 90°
 *    rotation maps [1, 0] to [0, 1].
 *
 * Sources & References:
 * ---------------------
 * - Wolfram MathWorld (Rotation Matrix): https://mathworld.wolfram.com/RotationMatrix.html
 *   (Defines standard rotation matrices where positive theta is CCW)
 * - Three.js Documentation (Euler): https://threejs.org/docs/#api/en/math/Euler
 *   (JotCAD's visualizer uses Three.js, which follows the RHS CCW convention)
 * - OpenGL Programming Guide: Chapter 3 (Mathematical operations follow RHS/CCW)
 */

describe('rotation direction', () => {
  it('rx(1/4) should rotate Y+ to Z+', async () => {
    // Looking from X+ towards origin, Y+ (up) rotates 90deg CCW to Z+ (towards viewer)
    await withTestAssets('rx_direction', async (assets) => {
      const p = Point(assets, 0, 1, 0).rotateX(1 / 4);
      const id = cgal.MakeAbsolute(assets, p);
      assert.strictEqual(assets.getText(id), 'V 1\nv 0 0 1 0 0 1\np 0\n');
    });
  });

  it('ry(1/4) should rotate Z+ to X+', async () => {
    // Looking from Y+ towards origin, Z+ (towards viewer) rotates 90deg CCW to X+ (right)
    await withTestAssets('ry_direction', async (assets) => {
      const p = Point(assets, 0, 0, 1).rotateY(1 / 4);
      const id = cgal.MakeAbsolute(assets, p);
      assert.strictEqual(assets.getText(id), 'V 1\nv 1 0 0 1 0 0\np 0\n');
    });
  });

  it('rz(1/4) should rotate X+ to Y+', async () => {
    // Looking from Z+ towards origin, X+ (right) rotates 90deg CCW to Y+ (up)
    await withTestAssets('rz_direction', async (assets) => {
      const p = Point(assets, 1, 0, 0).rotateZ(1 / 4);
      const id = cgal.MakeAbsolute(assets, p);
      assert.strictEqual(assets.getText(id), 'V 1\nv 0 1 0 0 1 0\np 0\n');
    });
  });
});
