import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Arc2 } from './arc.js';
import { Box3 } from './box.js';
import { Link } from './link.js';
import { Orb } from './orb.js';
import { Point } from './point.js';
import { Z } from './z.js';
import assert from 'node:assert/strict';
import { clean } from './clean.js';
import { extrude3 } from './extrude.js';
import { fill2 } from './fill.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { smooth } from './smooth.js';
import { z } from './z.js';

describe('clean op', () => {
  it('Arc2([-10, 10], { sides: 10 }).fill2().extrude3(Z(10)).clean()', async () => {
    await withTestSession('ops_test_clean_arc', async (session) => {
      await run(session, () =>
        Arc2([-10, 10], { sides: 10 })
          .fill2()
          .z(-5)
          .extrude3(Z(10))
          .clean({ angleThreshold: 0.12 }) // 54 degrees > 36 degrees - should merge wall facets
          .jot('observed.ops.test.clean_arc.jot')
          .png('observed.ops.test.clean_arc.png', [40, 40, 40], {
            wireframe: true,
          })
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.clean_arc.png',
          'observed.ops.test.clean_arc.png'
        )
      );
    });
  });

  it('Arc2([-10, 10], { sides: 10 }).fill2().extrude3(Z(10)).clean({ useAngleConstrained: false })', async () => {
    await withTestSession(
      'ops_test_clean_arc_unconstrained',
      async (session) => {
        await run(session, () =>
          Arc2([-10, 10], { sides: 10 })
            .fill2()
            .z(-5)
            .extrude3(Z(10))
            .clean({ angleThreshold: 0.12, useAngleConstrained: false })
            .jot('observed.ops.test.clean_arc_unconstrained.jot')
            .png(
              'observed.ops.test.clean_arc_unconstrained.png',
              [40, 40, 40],
              { wireframe: true }
            )
        );
        assert.ok(
          await testPng(
            session,
            'ops.test.clean_arc_unconstrained.png',
            'observed.ops.test.clean_arc_unconstrained.png'
          )
        );
      }
    );
  });
});
