import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';

import { Box3 } from './box.js';
import { Hull } from './hull.js';
import { Point } from './point.js';
import { strict as assert } from 'node:assert';
import { hull } from './hull.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { z } from './z.js';

describe('hull ops', () => {
  it('Hull() of points should create a 3D volume', async () =>
    await withTestSession('ops_test_hull_points', async (session) => {
      await run(session, () =>
        Hull(Point(0, 0, 0), Point(10, 0, 0), Point(0, 10, 0), Point(0, 0, 10))
          .png('observed.ops.test.hull_points.png', [100, 100, 100])
          .jot('observed.ops.test.hull_points.jot')
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.hull_points.jot',
          'observed.ops.test.hull_points.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.hull_points.png',
          'observed.ops.test.hull_points.png'
        )
      );
    }));

  it('hull() modifier should wrap multiple shapes', async () =>
    await withTestSession('ops_test_hull_modifier', async (session) => {
      await run(session, () =>
        Box3(2)
          .hull(Box3(2).z(10))
          .png('observed.ops.test.hull_modifier.png', [30, 30, 30])
          .jot('observed.ops.test.hull_modifier.jot')
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.hull_modifier.jot',
          'observed.ops.test.hull_modifier.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.hull_modifier.png',
          'observed.ops.test.hull_modifier.png'
        )
      );
    }));
});
