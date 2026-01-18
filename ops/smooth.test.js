import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Box3 } from './box.js';
import { Link } from './link.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { rz } from './rz.js';
import { smooth } from './smooth.js';

describe('smooth op', () => {
  it('Box3([10]).smooth(Link(Point(0, 0, -1), Point(0, 0, 11)))', async () => {
    await withTestSession('ops_test_smooth_box_z', async (session) => {
      await run(session, () =>
        Box3([30])
          .smooth(Link(Point(0, 0, -1), Point(0, 0, 31)), {
            radius: 10,
            angleThreshold: 15 / 360,
            resolution: 4,
            skipFairing: false,
            skipRefine: false,
            fairingContinuity: 1,
          })
          .png('observed.ops.test.smooth_box_z.png', [-100, -100, 100], {
            wireframe: true,
          })
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.smooth_box_z.png',
          'observed.ops.test.smooth_box_z.png'
        )
      );
    });
  });
});
