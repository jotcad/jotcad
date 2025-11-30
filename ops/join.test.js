import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { describe, it } from 'node:test';
import { testPng, withTestSession } from './test_session_util.js';

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { join } from './join.js';
import { png } from './png.js';
import { run } from '@jotcad/op';

describe('join', () => {
  it('should correctly join two boxes and render a PNG', async () =>
    await withTestSession('ops_test_join', async (session) => {
      await run(
        session,
        () =>
          Box3([0, 2], [0, 1], [0, 1])
            .join(Box3([0, 1], [0, 2], [0, 1]))
            .png('observed.ops.test.join.png', [3 * 2, 4 * 2, 5 * 2]) // Pass filename and view
      );
      assert.ok(
        await testPng(
          session,
          'ops.test.join.png',
          'observed.ops.test.join.png'
        )
      );
    }));
});
