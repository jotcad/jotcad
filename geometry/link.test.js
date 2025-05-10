import { cgal, cgalIsReady } from './getCgal.js';

import { Link } from './link.js';
import { Point } from './point.js';
import { renderPng } from './renderPng.js';
import test from 'ava';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

test('open', (t) =>
  withAssets(async (assets) => {
    const shape = Link(
      assets,
      [Point(assets, 1, 0, 0), Point(assets, 0, 1, 0), Point(assets, 0, 0, 1)],
      false,
      false
    );
    t.deepEqual(
      assets.text[shape.geometry],
      'v 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 1 1 2\n'
    );
    const image = await renderPng(assets, shape, {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    await writeFile('link.test.open.png', Buffer.from(image));
  }));

test('closed', (t) =>
  withAssets(async (assets) => {
    const shape = Link(
      assets,
      [Point(assets, 1, 0, 0), Point(assets, 0, 1, 0), Point(assets, 0, 0, 1)],
      true,
      false
    );
    t.deepEqual(
      assets.text[shape.geometry],
      'v 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 1 1 2 2 0\n'
    );
    const image = await renderPng(assets, shape, {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    await writeFile('link.test.closed.png', Buffer.from(image));
  }));

test('reverse', (t) =>
  withAssets(async (assets) => {
    const shape = Link(
      assets,
      [Point(assets, 1, 0, 0), Point(assets, 0, 1, 0), Point(assets, 0, 0, 1)],
      true,
      true
    );
    t.deepEqual(
      assets.text[shape.geometry],
      'v 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 2 2 1 1 0\n'
    );
    const image = await renderPng(assets, shape, {
      view: { position: [0, 0, 20] },
      width: 512,
      height: 512,
    });
    await writeFile('link.test.reverse.png', Buffer.from(image));
  }));
