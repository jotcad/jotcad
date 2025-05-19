import './intervalSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { cgal, cgalIsReady, testPng, withAssets } from '@jotcad/geometry';

import { Box2 } from './box.js';
import { color } from './color.js';
import { extrude } from './extrude.js';
import { fill } from './fill.js';
import { png } from './png.js';
import { readFile } from './fs.js';
import { run } from '@jotcad/op';
import { save } from './save.js';
import test from 'ava';
import { z } from './z.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('simple', async (t) =>
  withAssets(async (assets) => {
    const graph = await run(assets, () =>
      Box2([0, 3], [0, 5])
        .fill()
        .extrude(z(0), z(1))
        .color('blue')
        .png('observed.ops.test.simple.png', [-5, -5, 10])
    );
    t.true(await testPng('ops.test.simple.png'));
  }));
