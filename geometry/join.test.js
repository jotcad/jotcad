import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { join } from './join.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('join', () =>
  it('should join two boxes at a corner', async () => {
    await withTestAssets(
      'should join two boxes at a corner',
      async (assets) => {
        const box = Box3(assets, [0, 2], [0, 2], [0, 2]);
        const tool = Box3(assets, [1, 2], [1, 2], [1, 4]);
        const joinedBox = join(assets, box, [tool]);
        const image = await renderPng(assets, joinedBox, {
          view: { position: [15, 15, 15] },
          width: 512,
          height: 512,
        });
        assert.ok(
          await testPng(`${import.meta.dirname}/join.test.corner.png`, image)
        );
      }
    );
  }));
