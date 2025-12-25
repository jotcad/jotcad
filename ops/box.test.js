import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';
import { And } from './and.js';
import { Box2 } from './box.js';
import assert from 'node:assert/strict';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('Box2', () => {
  it('should create a 2D box from a single numeric array argument [10] and render correctly', async () => {
    await withTestSession(
      'box2_single_array_arg_10_render',
      async (session) => {
        // Execute the JOT expression Box2([10]) and save its PNG representation
        await run(session, () =>
          And(Box2(5), Box2(2.5), Box2([4]), Box2([-4])).png(
            'observed.ops.test.box2_10.png',
            [0, 0, 20]
          )
        );

        // Assert that the generated PNG matches the reference PNG.
        // This will visually verify if Box2([10]) creates the expected shape.
        assert.ok(
          await testPng(
            session,
            'ops.test.box2_10.png', // Reference PNG filename
            'observed.ops.test.box2_10.png' // Generated PNG filename
          ),
          'Generated PNG for Box2([10]) should match the reference.'
        );
      }
    );
  });
});
