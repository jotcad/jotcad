import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';

import { Box3 } from './box.js';
import { Z } from './z.js';
import assert from 'node:assert/strict';
import { clipOpen } from './clipOpen.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('clipOpen op', () => {
  it('should correctly clipOpen and render a PNG', async () =>
    await withTestSession('ops_test_clipOpen', async (session) => {
      await run(session, () =>
        Box3([0, 2], [0, 2], [0, 2])
          .clipOpen(Z(1))
          .png('observed.ops.test.clipOpen.png', [15, 15, 15])
      );

      assert.ok(
        await testPng(
          session,
          'ops.test.clipOpen.png',
          'observed.ops.test.clipOpen.png'
        )
      );
    }));
});
