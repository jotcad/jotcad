import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Box3 } from './box.js';
import { Z } from './z.js';
import assert from 'node:assert/strict';
import { cut3 } from './cut.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { ry } from './ry.js';
import { shell } from './shell.js';

describe('shell op', () => {
  it('Box3([10]).shell({ innerOffset: 0, outerOffset: 2, protect: false }).cut3(Z(0))', async () => {
    await withTestSession('ops_test_shell_box_cut', async (session) => {
      await run(session, () =>
        Box3([10])
          .shell({ innerOffset: 0, outerOffset: 2, protect: false })
          .ry(1 / 8)
          .cut3(Z(0))
          .ry(1 / 2)
          .png('observed.ops.test.shell_box_cut.png', [40, 40, 40])
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.shell_box_cut.png',
          'observed.ops.test.shell_box_cut.png'
        )
      );
    });
  });
});
