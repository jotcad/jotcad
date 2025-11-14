import { describe, it } from 'node:test';

import { Link } from './link.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { withAssets } from './assets.js';
import { writeFile } from 'node:fs/promises';

describe('link', () => {
  it('should produce an open linkage of points', async () => {
    await withAssets(async (assets) => {
      const shape = Link(
        assets,
        [
          Point(assets, 1, 0, 0),
          Point(assets, 0, 1, 0),
          Point(assets, 0, 0, 1),
        ],
        false,
        false
      );
      assert.deepEqual(
        assets.getText(shape.geometry),
        'V 3\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 1 1 2\n'
      );
      const image = await renderPng(assets, shape, {
        view: { position: [0, 0, 20] },
        width: 512,
        height: 512,
      });
      await writeFile('link.test.open.png', Buffer.from(image));
    });
  });

  it('should produce a closed linkage of points', async () => {
    await withAssets(async (assets) => {
      const shape = Link(
        assets,
        [
          Point(assets, 1, 0, 0),
          Point(assets, 0, 1, 0),
          Point(assets, 0, 0, 1),
        ],
        true,
        false
      );
      assert.deepEqual(
        assets.getText(shape.geometry),
        'V 3\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 1 1 2 2 0\n'
      );
      const image = await renderPng(assets, shape, {
        view: { position: [0, 0, 20] },
        width: 512,
        height: 512,
      });
      await writeFile('link.test.closed.png', Buffer.from(image));
    });
  });

  it('should produce a reversed linkage of points', async () => {
    await withAssets(async (assets) => {
      const shape = Link(
        assets,
        [
          Point(assets, 1, 0, 0),
          Point(assets, 0, 1, 0),
          Point(assets, 0, 0, 1),
        ],
        true,
        true
      );
      assert.deepEqual(
        assets.getText(shape.geometry),
        'V 3\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 2 2 1 1 0\n'
      );
      const image = await renderPng(assets, shape, {
        view: { position: [0, 0, 20] },
        width: 512,
        height: 512,
      });
      await writeFile('link.test.reverse.png', Buffer.from(image));
    });
  });
});