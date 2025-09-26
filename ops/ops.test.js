import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import * as fs from './fsNode.js';

import { cgal, testPng, withAssets } from '@jotcad/geometry';
import { describe, it } from 'node:test';

import { Box2 } from './box.js';
import assert from 'node:assert/strict';
import { color } from './color.js';
import { extrude2 } from './extrude.js';
import { fill2 } from './fill.js';
import { png } from './png.js';
import { readFile } from './fs.js';
import { run } from '@jotcad/op';
import { save } from './save.js';
import { withFs } from './fs.js';
import { z } from './z.js';

describe('ops', () => {
  it('should handle a simple chain', async () =>
    withFs(fs, async () => {
      await withAssets(async (assets) => {
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
});
