import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';

import { Box2 } from './box.js';
import { Point } from './point.js';
import { strict as assert } from 'node:assert';
import { grow } from './grow.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('grow op', () => {
  it('should grow a point into a box', async () => {
    await withTestSession('grow a point into a box (op)', async (session) => {
      await run(session, () =>
        Point([5, 5, 5])
          .grow(Box2([0, 2], [0, 2]))
          .png('observed.ops_grow_box.png')
      );
      assert.ok(
        await testPng(
          session,
          'ops.grow.test.box.png', // Expected image path
          'observed.ops_grow_box.png'
        )
      );
    });
  });
});
