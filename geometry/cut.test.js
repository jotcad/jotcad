import { describe, it } from 'node:test';

import { Arc2 } from './arc.js';
import { Box3 } from './box.js';
import assert from 'node:assert/strict';
import { cut2, cut3 } from './cut.js';
import { fill2 } from './fill.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';
import { withTestAssets } from './test_session_util.js';

describe('cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withTestAssets(
      'should cut the corner out of a box',
      async (assets) => {
        const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
        const tool = await Box3(assets, [1, 2], [1, 2], [1, 4]);
        const cutBox = cut3(assets, box, [tool]);
        const image = await renderPng(assets, cutBox, {
          view: { position: [15, 15, 15] },
          width: 512,
          height: 512,
        });
        assert.ok(
          await testPng(`${import.meta.dirname}/cut.test.corner.png`, image)
        );
      }
    );
  }));

describe('masked cut', (t) =>
  it('should cut the corner out of a box', async () => {
    await withTestAssets(
      'should cut the corner out of a box (masked)',
      async (assets) => {
        const box = await Box3(assets, [0, 2], [0, 2], [0, 2]);
        const tool = await Box3(assets, [1, 2], [1, 2], [1, 2]);
        const mask = await Box3(assets, [0, 2], [1, 2], [1, 2]);
        const maskedTool = tool.derive({ mask });
        const cutBox = cut3(assets, box, [maskedTool]);
        const image = await renderPng(assets, cutBox, {
          view: { position: [15, 15, 15] },
          width: 512,
          height: 512,
        });
        assert.ok(
          await testPng(`${import.meta.dirname}/cut.test.masked_cut.png`, image)
        );
      }
    );
  }));

describe('cut filled circle', (t) =>
  it('should produce a filled ring', async () => {
    await withTestAssets('should produce a filled ring', async (assets) => {
      const outer = fill2(assets, [Arc2(assets, [-40, 40], [-40, 40])]);
      const inner = fill2(assets, [Arc2(assets, [-30, 30], [-30, 30])]);
      const ring = cut2(assets, outer, [inner]);
      const image = await renderPng(assets, ring, {
        view: { position: [0, 0, 150] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/cut.test.ring.png`, image)
      );
    });
  }));
