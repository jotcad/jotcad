import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';
import { Arc2 } from './arc.js';
import { Box2 } from './box.js';
import { Box3 } from './box.js';
import { Curve } from './curve.js';
import { Point } from './point.js';
import { Sweep } from './sweep.js';
import assert from 'node:assert/strict';
import { debug } from './debug.js';
import { fill2 } from './fill.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { x } from './x.js';
import { y } from './y.js';
import { z } from './z.js';

describe('sweep op coverage', () => {
  it('Strategy: Round - Semi-circle Path', async () => {
    await withTestSession('sweep_round_semi_circle_path', async (session) => {
      let result;
      await run(session, () => {
        const path = Arc2([-10, 10], [-10, 10], { start: 0, end: 0.5 });
        const profile = Box2([-1, 1], [-1, 1]);
        return Sweep(path, [profile], { strategy: 'round', minTurnRadius: 1.5 })
          .debug((shape) => (result = shape))
          .z(0)
          .y(-5)
          .png('observed.sweep_round_semi_circle_path.png', [40, 0, 20], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_semi_circle_path.png',
          'observed.sweep_round_semi_circle_path.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, undefined);
    });
  });

  it('Strategy: Round - Box Profile 45 Degree Turn', async () => {
    await withTestSession('sweep_round_box_45', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([
          Point(0, 0, 0),
          Point(10, 0, 0),
          Point(17.07, 7.07, 0),
        ]);
        const profile = Box2([-1, 1], [-1, 1]);
        return Sweep(path, [profile], { strategy: 'round' })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_box_45.png', [10, 0, 30], {
            target: [10, 0, 0],
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_box_45.png',
          'observed.sweep_round_box_45.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, undefined);
    });
  });

  it('Strategy: Round - Box Profile Line', async () => {
    await withTestSession('sweep_round_box_45', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([Point(0, 0, 0), Point(10, 0, 0)]);
        const profile = Box2([-1, 1], [-1, 1]);
        return Sweep(path, [profile], { strategy: 'round' })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_box_line.png', [0, 50, 20], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_box_line.png',
          'observed.sweep_round_box_line.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, undefined);
    });
  });

  it('Strategy: Round - Large Min Turn Radius', async () => {
    await withTestSession('sweep_round_large_radius', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([Point(0, 0, 0), Point(20, 0, 0), Point(20, 20, 0)]);
        const profile = Box2([-1, 1], [-1, 1]);
        // Large radius (10) should result in a very broad curve compared to default
        return Sweep(path, [profile], { strategy: 'round', minTurnRadius: 10 })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_large_radius.png', [80, 10, 80], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_large_radius.png',
          'observed.sweep_round_large_radius.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, ['selfIntersecting']);
    });
  });

  it('Strategy: Round - Closed Loop Rectangle', async () => {
    await withTestSession('sweep_round_closed_rect', async (session) => {
      let result;
      await run(session, () => {
        // Closed path should produce rounded corners at all 4 vertices
        const path = Curve(
          [Point(0, 0, 0), Point(20, 0, 0), Point(20, 20, 0), Point(0, 20, 0)],
          true
        );
        const profile = Arc2(2);
        return Sweep(path, [profile], {
          strategy: 'round',
          closedPath: true,
          minTurnRadius: 2.1,
        })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_closed_rect.png', [20, 20, 20], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_closed_rect.png',
          'observed.sweep_round_closed_rect.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, ['selfIntersecting']);
    });
  });

  it('Strategy: Round - Sharp 135 Degree Turn', async () => {
    await withTestSession('sweep_round_sharp_135', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([Point(0, 0, 0), Point(10, 0, 0), Point(3, 7, 0)]);
        const profile = Box2([-0.5, 0.5], [-0.5, 0.5]);
        return Sweep(path, [profile], { strategy: 'round', minTurnRadius: 5.0 })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_sharp_135.png', [10, 0, 40], {
            target: [10, 0, 0],
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_sharp_135.png',
          'observed.sweep_round_sharp_135.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, ['selfIntersecting']);
    });
  });

  it('Strategy: Round - 3D Path', async () => {
    await withTestSession('sweep_round_3d_path', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([
          Point(0, 0, 0),
          Point(10, 0, 0),
          Point(10, 10, 10),
        ]);
        const profile = Arc2(1);
        return Sweep(path, [profile], { strategy: 'round', minTurnRadius: 1.5 })
          .debug((shape) => (result = shape))
          .x(-10)
          .png('observed.sweep_round_3d_path.png', [10, 0, 20], {
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_3d_path.png',
          'observed.sweep_round_3d_path.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, undefined);
    });
  });

  it('Strategy: Round - Small Min Turn Radius', async () => {
    await withTestSession('sweep_round_small_radius', async (session) => {
      let result;
      await run(session, () => {
        const path = Curve([Point(0, 0, 0), Point(10, 10, 0), Point(20, 0, 0)]);
        const profile = Box2([-1, 1], [-1, 1]);
        return Sweep(path, [profile], {
          strategy: 'round',
          minTurnRadius: 0.05,
        })
          .debug((shape) => (result = shape))
          .z(5)
          .png('observed.sweep_round_small_radius.png', [40, 0, 40], {
            target: [10, 5, 0],
            wireframe: true,
          });
      });
      assert.ok(
        await testPng(
          session,
          'sweep_round_small_radius.png',
          'observed.sweep_round_small_radius.png'
        )
      );
      assert.deepEqual(result?.tags?.invalid, undefined);
    });
  });
});
