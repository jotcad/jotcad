import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import * as fs from './fsNode.js';

import { cgal, testPng, withAssets } from '@jotcad/geometry';
import { describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import { Box2 } from './box.js';
import { Orb } from './orb.js';
import assert from 'node:assert/strict';
import { color } from './color.js';
import { cut } from './cut.js';
import { extrude2 } from './extrude.js';
import { fill2 } from './fill.js';
import { jot } from './jot.js';
import { png } from './png.js';
import { readFile } from './fs.js';
import { run } from '@jotcad/op';
import { save } from './save.js';
import { withFs } from './fs.js';
import { z } from './z.js';

describe('ops', () => {
  /*
  it('trivial', async () =>
    withFs(fs, async () => {
      await withAssets('ops_test_simple_chain', async (assets) => {
        const graph = await run(assets, () =>
          // This test is fundamentally flawed. It tries to use a 3D `z` op on a 2D `Box2`.
          Box2([0, 3], [0, 5])
            .cut(z(0))
            .png('observed.ops.test.trivial.png', [-20, -20, 40])
        );
        assert.ok(await testPng('ops.test.trivial.png'));
      });
    }));
  */
  /*
  it('should handle a simple chain', async () =>
    withFs(fs, async () => {
      await withAssets('ops_test_simple_chain', async (assets) => {
        const graph = await run(assets, () =>
          Box2([0, 3], [0, 5])
            .fill2()
            .extrude2(z(0), z(1))
            .color('blue')
            .png('observed.ops.test.simple.png', [-20, -20, 40])
        );
        assert.ok(await testPng('ops.test.simple.png'));
      });
    }));
  */
  /*
  it('hemisphere', async () =>
    withFs(fs, async () => {
      await withAssets('hemisphere', async (assets) => {
        const graph = await run(assets, () =>
          Orb(30)
            .cut(z(1))
            .png('observed.ops.test.hemisphere.png', [-20, -20, 40])
        );
        assert.ok(await testPng('ops.test.hemisphere.png'));
      });
    }));
  */
  it('Orb(11).cut(z(1))', async () =>
    withFs(fs, async () => {
      await withAssets('ops_test_failing_op', async (assets) => {
        const graph = await run(assets, () => Orb(12).cut(z(1)));
        assert.ok(graph);
      });
    }));

  /*
  it("Arc2(10).jot('out.jot')", async () =>
    withFs(fs, async () => {
      await withAssets('ops_test_arc2_jot', async (assets) => {
        const graph = await run(assets, () => Arc2(10).jot('out.jot'));
        assert.ok(graph);
        const content = await fs.readFile('out.jot', { encoding: 'utf8' });
        assert.ok(content.length > 0);
      });
    }));
  */
});
