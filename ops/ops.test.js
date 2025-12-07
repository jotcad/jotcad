import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import * as fs from './fsNode.js';

import { describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import { Box2 } from './box.js';
import { Orb } from './orb.js';
import { and } from './and.js';
import assert from 'node:assert/strict';
import { color } from './color.js';
import { cut } from './cut.js';
import { extrude2 } from './extrude.js';
import { fill2 } from './fill.js';
import { footprint } from './footprint.js';
import { grow } from './grow.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { readFile } from './fs.js';
import { run } from '@jotcad/op';
import { save } from './save.js';
import { testPng } from '@jotcad/geometry';
import { withFs } from './fs.js';
import { withTestSession } from './test_session_util.js';
import { z } from './z.js';

describe('ops', () => {
  /*
  it('trivial', async () =>
    withFs(fs, async () => {
      await withTestSession('ops_test_simple_chain', async (session) => {
        const graph = await run(session, () =>
          // This test is fundamentally flawed. It tries to use a 3D `z` op on a 2D `Box2`.
          Box2([0, 3], [0, 5])
            .cut(z(0))
            .png('observed.ops.test.trivial.png', [-20, -20, 40])
        );
        assert.ok(await testPng(session.assets, 'ops.test.trivial.png'));
      });
    }));
  */
  /*
  it('should handle a simple chain', async () =>
    withFs(fs, async () => {
      await withTestSession('ops_test_simple_chain', async (session) => {
        const graph = await run(session, () =>
          Box2([0, 3], [0, 5])
            .fill2()
            .extrude2(z(0), z(1))
            .color('blue')
            .png('observed.ops.test.simple.png', [-20, -20, 40])
        );
        assert.ok(await testPng(session.assets, 'ops.test.simple.png'));
      });
    }));
  */
  /*
  it('hemisphere', async () =>
    withFs(fs, async () => {
      await withTestSession('hemisphere', async (session) => {
        const graph = await run(session, () =>
          Orb(30)
            .cut(z(1))
            .png('observed.ops.test.hemisphere.png', [-20, -20, 40])
        );
        assert.ok(await testPng(session.assets, 'ops.test.hemisphere.png'));
      });
    }));
  */
  it('Orb(11).cut(z(1))', async () =>
    withFs(fs, async () => {
      await withTestSession('ops_test_failing_op', async (session) => {
        const graph = await run(session, () => Orb(12).cut(z(1)));
        assert.ok(graph);
      });
    }));

  it('Orb(10).z(10).and(footprint())', async () =>
    withFs(fs, async () => {
      await withTestSession('ops_test_orb_footprint', async (session) => {
        const graph = await run(session, () =>
          Orb(10)
            .z(10)
            .and(footprint())
            .png('observed.ops.test.orb_footprint.png', [-20, -20, 40])
        );
        assert.ok(graph);
      });
    }));

  /*
  it("Arc2(10).jot('out.jot')", async () =>
    withFs(fs, async () => {
      await withTestSession('ops_test_arc2_jot', async (session) => {
        const graph = await run(session, () => Arc2(10).jot('out.jot'));
        assert.ok(graph);
        const content = await fs.readFile('out.jot', { encoding: 'utf8' });
        assert.ok(content.length > 0);
      });
    }));
  */
});
