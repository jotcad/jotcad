import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';

import { Box3 } from './box.js';
import { strict as assert } from 'node:assert';
import { cut } from './cut.js';
import { footprint } from './footprint.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('footprint op', () => {
  it('should create a 2D footprint from a 3D box', async () => {
    await withTestSession('footprint of a 3d box (op)', async (session) => {
      await run(session, () =>
        Box3([0, 10], [0, 10], [0, 10])
          .footprint()
          .png('observed.ops_footprint_box.png')
      );
      assert.ok(
        await testPng(
          session,
          'ops.footprint.test.box.png', // Expected image path
          'observed.ops_footprint_box.png'
        )
      );
    });
  });

  it('should create a 2D footprint from an object with a hole', async () => {
    await withTestSession(
      'footprint of an object with a hole (op)',
      async (session) => {
        await run(session, () =>
          Box3([-10, 10], [-10, 10], [0, 1])
            .cut(Box3([-5, 5], [-5, 5], [0, 1]))
            .footprint()
            .png('observed.ops_footprint_hollow_square.png')
        );
        assert.ok(
          await testPng(
            session,
            'ops.footprint.test.hollow_square.png', // Expected image path
            'observed.ops_footprint_hollow_square.png'
          )
        );
      }
    );
  });
});
