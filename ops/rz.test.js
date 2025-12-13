import * as fs from './fsNode.js';

import { Box2, Box3 } from './box.js';
import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';

import { strict as assert } from 'node:assert';
import { color } from './color.js';
import { join } from './join.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { rz } from './rz.js';
import { x } from './x.js';

describe('rz op with by parameter', () => {
  it('should produce 8 boxes in a circle using rz with by parameter', async () =>
    await withTestSession('ops_test_boxes_in_circle', async (session) => {
      await run(session, () =>
        Box2(1)
          .x(1)
          .rz({ by: 1 / 8 })
          .png('observed.ops.test.boxes_in_circle.png', [-2, -2, 4])
          .jot('observed.ops.test.boxes_in_circle.jot')
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.boxes_in_circle.jot',
          'observed.ops.test.boxes_in_circle.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.boxes_in_circle.png',
          'observed.ops.test.boxes_in_circle.png'
        )
      );
    }));
});

describe('ops/rz_color', () => {
  it('Box3(1).x(1).rz(0, 0.25).color("green") should generate the correct image', async () => {
    await withTestSession('ops_rz_color_box_green_test', async (session) => {
      await run(session, () =>
        Box3(10)
          .x(10)
          .rz({ by: 1 / 8, to: 2 })
          .color('green')
          .png('observed.rz_color_box_green.png')
          .jot('observed.rz_color_box_green.jot')
      );
      assert.ok(
        await testPng(
          session,
          'rz_color_box_green.png',
          'observed.rz_color_box_green.png'
        )
      );
      assert.ok(
        await testJot(
          session,
          'rz_color_box_green.jot',
          'observed.rz_color_box_green.jot'
        )
      );
    });
  });

  it('Box3(1).x(1).rz(0, 0.25).join().color("green") should generate the correct image', async () => {
    await withTestSession(
      'ops_rz_color_box_join_green_test',
      async (session) => {
        await run(session, () =>
          Box3(10)
            .x(10)
            .rz({ by: 1 / 8 })
            .join()
            .color('green')
            .png('observed.rz_color_box_join_green.png')
            .jot('observed.rz_color_box_join_green.jot')
        );
        assert.ok(
          await testPng(
            session,
            'rz_color_box_join_green.png',
            'observed.rz_color_box_join_green.png'
          )
        );
        assert.ok(
          await testJot(
            session,
            'rz_color_box_join_green.jot',
            'observed.rz_color_box_join_green.jot'
          )
        );
      }
    );
  });
});

describe('rz op with by parameter comparison', () => {
  it('should produce the same jot output for rz(0/2, 1/2) and rz({ by: 1/2 })', async () =>
    await withTestSession('ops_test_rz_by_compare', async (session) => {
      // Generate JOT for rz(0/2, 1/2)
      await run(session, () =>
        Box2(1)
          .x(1)
          .rz(0 / 2, 1 / 2)
          .jot('observed.rz_normal.jot')
      );

      // Generate JOT for rz({ by: 1/2 })
      await run(session, () =>
        Box2(1)
          .x(1)
          .rz({ by: 1 / 2 })
          .jot('observed.rz_by.jot')
      );

      assert.ok(
        await testJot(session, 'rz_normal.jot', 'observed.rz_normal.jot')
      );
      assert.ok(await testJot(session, 'rz_by.jot', 'observed.rz_by.jot'));
    }));
});
