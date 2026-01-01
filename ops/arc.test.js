import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { And } from './and.js';
import { Arc2 } from './arc.js';
import assert from 'node:assert/strict';
import { fill2 } from './fill.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('Arc2', () => {
  it('should create a 2D arc from a single numeric array argument [10] and render correctly', async () => {
    await withTestSession(
      'arc2_single_array_arg_10_render',
      async (session) => {
        await run(session, () =>
          And(Arc2(5), Arc2(2.5), Arc2([4]), Arc2([-4]))
            .jot('observed.ops.test.arc2_10.jot')
            .png('observed.ops.test.arc2_10.png', [0, 0, 20])
        );

        // Assert that the generated JOT matches the reference JOT.
        assert.ok(
          await testJot(
            session,
            'ops.test.arc2_10.jot',
            'observed.ops.test.arc2_10.jot'
          )
        );

        // Assert that the generated PNG matches the reference PNG.
        assert.ok(
          await testPng(
            session,
            'ops.test.arc2_10.png', // Reference PNG filename
            'observed.ops.test.arc2_10.png' // Generated PNG filename
          ),
          'Generated PNG for Arc2([10]) should match the reference.'
        );
      }
    );
  });

  it('Arc2(10).fill2() should produce a filled 2D arc', async () => {
    await withTestSession('arc2_fill_10_render', async (session) => {
      await run(session, () =>
        Arc2(10)
          .fill2()
          .jot('observed.ops.test.arc2_fill_10.jot')
          .png('observed.ops.test.arc2_fill_10.png', [0, 0, 20])
      );

      assert.ok(
        await testJot(
          session,
          'ops.test.arc2_fill_10.jot',
          'observed.ops.test.arc2_fill_10.jot'
        )
      );

      assert.ok(
        await testPng(
          session,
          'ops.test.arc2_fill_10.png',
          'observed.ops.test.arc2_fill_10.png'
        )
      );
    });
  });
});
