import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';

import { Box2 } from './box.js';
import assert from 'node:assert/strict';
import { jot } from './jot.js';
import { png } from './png.js';
import { rule } from './rule.js';
import { run } from '@jotcad/op';
import { z } from './z.js';

describe('ops/rule', () => {
  it('Box2(10).rule(z(4)) should generate the correct image', async () => {
    await withTestSession('ops_rule_box2_z_test', async (session) => {
      await run(session, () =>
        Box2(10)
          .rule(z(4))
          .png('observed.rule.Box2_z.png')
          .jot('observed.rule.Box2_z.jot')
      );
      assert.ok(
        await testPng(session, 'rule.Box2_z.png', 'observed.rule.Box2_z.png')
      );
      assert.ok(
        await testJot(session, 'rule.Box2_z.jot', 'observed.rule.Box2_z.jot')
      );
    });
  });
});
