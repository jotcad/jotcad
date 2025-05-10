import { cgal, cgalIsReady } from '@jotcad/geometry';

import { Box2 } from './box.js';
import { color } from './color.js';
import { extrude } from './extrude.js';
import { fill } from './fill.js';
import { png } from './png.js';
import { readFile } from 'node:fs/promises';
import { run } from '@jotcad/op';
import { save } from './save.js';
import test from 'ava';
import { z } from './z.js';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('simple', async (t) => {
  const assets = { text: {} };
  const graph = await run(assets, () =>
    Box2([0, 0], [3, 5])
      .fill()
      .extrude(z(0), z(1))
      .color('blue')
      .save('ops.test.simple.json')
      .png('ops.test.simple.png', [-5, -5, 10])
  );
  t.deepEqual(
    JSON.parse(await readFile('ops.test.simple.json', { encoding: 'utf8' })),
    {
      shapes: [
        {
          geometry:
            'd92318f35bba6cfdeeee3ed63a30220aeac4bf1c9cbbb2087383849985d77b07',
          tags: { color: 'blue' },
        },
      ],
      tags: { color: 'blue' },
    }
  );
});
