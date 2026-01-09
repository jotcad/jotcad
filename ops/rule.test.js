import { describe, it } from 'node:test';
import { testJot, testPng, withTestSession } from './test_session_util.js';
import { Arc2 } from './arc.js';
import { Rule } from './rule.js';
import assert from 'node:assert/strict';
import { close3 } from './close.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { run } from '@jotcad/op';
import { z } from './z.js';

describe('rule orientation', () => {
  it('should produce a consistently oriented mesh for a complex stack', async () => {
    await withTestSession('ops_test_rule_orientation', async (session) => {
      await run(session, () =>
        Rule(
          Arc2(50),
          Arc2(48).z(1),
          Arc2(46).z(0.5),
          Arc2(17).z(0.5),
          Arc2(15).z(1.5),
          Arc2(10.5).z(1.5),
          Arc2(10.5).z(-1.5),
          Arc2(15).z(-1.5),
          Arc2(17).z(-0.5),
          Arc2(46).z(-0.5),
          Arc2(48).z(-1),
          Arc2(50)
        )
          .close3()
          .jot('observed.ops.test.rule_oriented.jot')
          .png('observed.ops.test.rule_oriented.png', [200, 200, 200])
      );
      assert.ok(
        await testJot(
          session,
          'ops.test.rule_oriented.jot',
          'observed.ops.test.rule_oriented.jot'
        )
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.rule_oriented.png',
          'observed.ops.test.rule_oriented.png'
        )
      );
    });
  });
});
