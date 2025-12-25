import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';
import { And } from './and.js';
import { Arc2 } from './arc.js';
import assert from 'node:assert/strict';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('Arc2', () => {
  it('should create a 2D arc from a single numeric array argument [10] and render correctly', async () => {
    await withTestSession(
      'arc2_single_array_arg_10_render',
      async (session) => {
        await run(session, () =>
          And(Arc2(5), Arc2(2.5), Arc2([4]), Arc2([-4])).png(
            'observed.ops.test.arc2_10.png',
            [0, 0, 20]
          )
        );

        // Assert that the generated PNG matches the reference PNG.
        // This will visually verify if Arc2([10]) creates the expected shape.
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
});
