import { describe, it } from 'node:test';
import { fileURLToPath } from 'url';
import path, { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

import { Point } from './point.js';
import assert from 'node:assert/strict';
import { cgal } from './getCgal.js';
import { makeShape } from './shape.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';
import { writeFile } from 'node:fs/promises';

describe('renderPng', () =>
  it('should render a triangle', async () => {
    await withTestAssets('should render a triangle', async (assets) => {
      const id = cgal.Link(
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
        assets.getText(id),
        'V 3\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nv 0 0 1 0 0 1\ns 0 2 2 1 1 0\n'
      );
      const triangle = makeShape({ geometry: id });
      const image = await renderPng(assets, triangle, {
        width: 512,
        height: 512,
      });
      assert.ok(await testPng(assets, 'renderPng.test.triangle.png', image));
    });
  }));
