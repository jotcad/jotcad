import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Curve } from './curve.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { curve } from './curve.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('curve op', () => {
  it('Curve([p1, p2, p3, p4])', async () => {
    await withTestSession('ops_test_curve_explicit', async (session) => {
      await run(session, () =>
        Curve([
          Point(0, 0, 0),
          Point(10, 10, 0),
          Point(20, 0, 0),
          Point(30, 10, 0),
        ])
          .jot('observed.ops.test.curve_explicit.jot')
          .png('observed.ops.test.curve_explicit.png', [60, 60, 60], {
            wireframe: true,
          })
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.curve_explicit.jot',
          'observed.ops.test.curve_explicit.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.curve_explicit.png',
          'observed.ops.test.curve_explicit.png'
        )
      );
    });
  });

  it('Point(0,0,0).curve([p2, p3, p4])', async () => {
    await withTestSession('ops_test_curve_chained', async (session) => {
      await run(session, () =>
        Point(0, 0, 0)
          .curve([Point(10, 10, 0), Point(20, 0, 0), Point(30, 10, 0)])
          .jot('observed.ops.test.curve_chained.jot')
          .png('observed.ops.test.curve_chained.png', [60, 60, 60], {
            wireframe: true,
          })
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.curve_chained.jot',
          'observed.ops.test.curve_chained.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.curve_chained.png',
          'observed.ops.test.curve_chained.png'
        )
      );
    });
  });

  it('Curve([...], { closed: true })', async () => {
    await withTestSession('ops_test_curve_closed', async (session) => {
      await run(session, () =>
        Curve(
          [
            Point(0, 0, 0),
            Point(10, 10, 0),
            Point(20, 0, 0),
            Point(10, -10, 0),
          ],
          { closed: true }
        )
          .jot('observed.ops.test.curve_closed.jot')
          .png('observed.ops.test.curve_closed.png', [60, 60, 60], {
            wireframe: true,
          })
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.curve_closed.jot',
          'observed.ops.test.curve_closed.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.curve_closed.png',
          'observed.ops.test.curve_closed.png'
        )
      );
    });
  });

  it('Curve([p1, p2, p3, p4]) with 3D point', async () => {
    await withTestSession('ops_test_curve_3d', async (session) => {
      await run(session, () =>
        Curve([
          Point(0, 0, 0),
          Point(10, 10, 10),
          Point(20, 0, 0),
          Point(30, 10, -10),
        ])
          .jot('observed.ops.test.curve_3d.jot')
          .png('observed.ops.test.curve_3d.png', [60, 60, 60], {
            wireframe: true,
          })
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.curve_3d.jot',
          'observed.ops.test.curve_3d.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.curve_3d.png',
          'observed.ops.test.curve_3d.png'
        )
      );
    });
  });

  it('Curve([p1]) should throw error', async () => {
    await withTestSession('ops_test_curve_error', async (session) => {
      try {
        await run(session, () => Curve([Point(0, 0, 0)]));
        assert.fail('Should have thrown error');
      } catch (e) {
        assert.equal(e.message, 'Curve requires at least 2 points.');
      }
    });
  });
});
