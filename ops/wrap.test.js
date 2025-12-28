import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';

import { Arc2 } from './arc.js'; // ADDED: Import Arc2
import { Box3 } from './box.js'; // Assuming Box3 is needed as input
import { Rule } from './rule.js'; // ADDED: Import Rule
import assert from 'node:assert/strict';
import { jot } from './jot.js'; // Import jot for chaining
import { png } from './png.js'; // MODIFIED: Import png for chaining
import { run } from '@jotcad/op'; // Import run
import { wrap3 } from './wrap.js'; // MODIFIED: Import wrap3
import { z } from './z.js'; // ADDED: Import z

describe('wrap3 (ops)', () => {
  // MODIFIED: describe block name
  it('should create a 3D alpha wrap around a box and produce jot output and png', async () => {
    // MODIFIED: Test description
    await withTestSession('ops_wrap_box_test', async (session) => {
      // Apply Wrap using the spec: ['shape', ['number', 'number'], 'shape']
      const alpha = 5.0;
      const offset = 1.0;

      // Execute the high-level JOT wrap3 operation within run()
      // Chain .png() and .jot() directly onto the expression
      await run(
        session,
        () =>
          Box3([-5, 5], [-5, 5], [-5, 5]) // MODIFIED: Box3 call
            .wrap3(alpha, offset) // MODIFIED: Chain the wrap3 operation with distinct arguments
            .png('observed.ops.wrap.test.box.png', {
              // ADDED: PNG output
              view: { position: [15, 15, 15] },
              width: 512,
              height: 512,
            })
            .jot('observed.ops.wrap.test.box.jot') // Chain jot to save output
      );

      // Assert that the generated JOT matches the reference JOT.
      assert.ok(
        await testJot(
          session, // Pass the session object
          'ops.wrap.test.box.jot', // Reference JOT filename
          'observed.ops.wrap.test.box.jot' // Generated JOT filename
        ),
        'Generated JOT for ops.wrap3 around a box should match the reference.'
      );

      // ADDED: Assert that the generated PNG matches the reference PNG.
      assert.ok(
        await testPng(
          session, // Pass the session object
          'ops.wrap.test.box.png', // Reference PNG filename
          'observed.ops.wrap.test.box.png' // Generated PNG filename
        ),
        'Generated PNG for ops.wrap3 around a box should match the reference.'
      );
    });
  });

  // ADDED: New test case for Rule().wrap3()
  it('should alpha wrap a ruled surface (Rule().wrap3()) and produce jot output and png', async () => {
    await withTestSession('ops_wrap_rule_test', async (session) => {
      const alpha = 5.0;
      const offset = 1.0;

      await run(session, () =>
        Rule(
          Arc2(1).z(5),
          Arc2(25).z(5),
          Arc2(30).z(0),
          Arc2(47).z(20),
          Arc2(57).z(35),
          Arc2(65).z(50),
          Arc2(67).z(60),
          Arc2(69).z(62)
        )
          .wrap3(alpha, offset)
          .png('observed.ops.wrap.test.rule_wrap3.png', [0, 200, 200])
          .jot('observed.ops.wrap.test.rule_wrap3.jot')
      );

      assert.ok(
        await testJot(
          session,
          'ops.wrap.test.rule_wrap3.jot',
          'observed.ops.wrap.test.rule_wrap3.jot'
        ),
        'Generated JOT for ops.wrap3 on ruled surface should match the reference.'
      );

      assert.ok(
        await testPng(
          session,
          'ops.wrap.test.rule_wrap3.png',
          'observed.ops.wrap.test.rule_wrap3.png'
        ),
        'Generated PNG for ops.wrap3 on ruled surface should match the reference.'
      );
    });
  });

  // Add more test cases for different scenarios if needed
});
