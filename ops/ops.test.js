import { cgal, cgalIsReady, shape } from '@jotcad/geometry';

import { Box2 } from './box.js';
import { color } from './color.js';
import { png } from './png.js';
import { read } from '@jotcad/sys';
import { run } from '@jotcad/op';
import { save } from './save.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('simple', async (t) => {
  const assets = { text: {} };
  const graph = await run(assets, () =>
    Box2([0, 0], [10, 20])
      .color('blue')
      .save('out')
      .png('ops.test.simple.png', [0, 0, 10])
  );
  t.deepEqual(
    await read('shape/out'),
    shape({
      geometry:
        '3108b88e27dee135a200cb514e65bb1f9fd4ff058daa16d7565bc10c6286ed63',
      tags: {
        color: 'blue',
      },
      tf: 's 10 20 1 10 20 1',
    })
  );
});
