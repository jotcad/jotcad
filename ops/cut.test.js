import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Arc2 } from './arc.js';
import assert from 'node:assert/strict';
import { cut2 } from './cut.js';
import { fill2 } from './fill.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('cut', () => {
  it('Arc2(40).fill2().cut2(Arc2(30).fill2())', async () => {
    await withTestSession('ops_test_ring_fill', async (session) => {
      await run(session, () =>
        Arc2(40)
          .fill2()
          .cut2(Arc2(30).fill2())
          .jot('observed.ops.test.ring_fill.jot')
          .png('observed.ops.test.ring_fill.png', [0, 0, 150])
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.ring_fill.jot',
          'observed.ops.test.ring_fill.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.ring_fill.png',
          'observed.ops.test.ring_fill.png'
        )
      );
    });
  });
});
