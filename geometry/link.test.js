import { describe, it } from 'node:test';
import { fileURLToPath } from 'url';
import path, { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import { Link } from './link.js';
import { Point } from './point.js';
import assert from 'node:assert/strict';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';
import { writeFile } from 'node:fs/promises';

describe('link', () => {
  it('should produce an open linkage of points', async () => {
    await withTestAssets(
      'should produce an open linkage of points',
      async (assets) => {
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
        assert.ok(await testPng(assets, 'link.test.open.png', image));
      }
    );
  });

  it('should produce a closed linkage of points', async () => {
    await withTestAssets(
      'should produce a closed linkage of points',
      async (assets) => {
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
        assert.ok(await testPng(assets, 'link.test.closed.png', image));
      }
    );
  });

  it('should produce a reversed linkage of points', async () => {
    await withTestAssets(
      'should produce a reversed linkage of points',
      async (assets) => {
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
        assert.ok(await testPng(assets, 'link.test.reverse.png', image));
      }
    );
  });
});
