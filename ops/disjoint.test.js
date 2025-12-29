import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';

import { Arc2 } from './arc.js';
import { Disjoint } from './disjoint.js';
import { Orb } from './orb.js';
import assert from 'node:assert/strict';
import { color } from './color.js';
import { png } from './png.js';
import { rule } from './rule.js';
import { run } from '@jotcad/op';
import { rx } from './rx.js';
import { ry } from './ry.js';
import { rz } from './rz.js';
import { set } from './set.js';
import { z } from './z.js';

describe('Disjoint', () => {
  it('should create disjoint shapes from orb and ruled surface', async () =>
    await withTestSession('ops_test_disjoint', async (session) => {
      await run(session, () =>
        Disjoint(
          Orb(80).color('yellow').set('id', 'a'),
          Arc2(20)
            .z(38)
            .rule(Arc2(15).z(42))
            .set('id', 'b')
            .ry(5 / 32)
        )
          .rz(1 / 8)
          .rx(1 / 16)
          .ry(1 / 16)
          .png('observed.ops.test.disjoint.png', [150, 150, 150])
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.disjoint.png',
          'observed.ops.test.disjoint.png'
        )
      );
    }));
});
