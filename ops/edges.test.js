import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Arc2 } from './arc.js';
import { Box3 } from './box.js';
import { Z } from './z.js';
import assert from 'node:assert/strict';
import { edges } from './edges.js';
import { extrude3 } from './extrude.js';
import { fill2 } from './fill.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('edges op', () => {
  it('Arc2([-10, 10], { sides: 10 }).fill2().extrude3(Z(10)).edges()', async () => {
    await withTestSession('ops_test_edges_arc', async (session) => {
      await run(session, () =>
        Arc2([-10, 10], { sides: 10 })
          .fill2()
          .extrude3(Z(10))
          .edges({ angleThreshold: 0.2 }) // 18 degrees < 36 degrees - should catch all vertical edges
          .jot('observed.ops.test.edges_arc.jot')
          .png('observed.ops.test.edges_arc.png', [40, 40, 40])
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.edges_arc.png',
          'observed.ops.test.edges_arc.png'
        )
      );
    });
  });
});
