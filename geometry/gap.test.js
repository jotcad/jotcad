import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { Box3 } from './box.js';
import { join } from './join.js';
import { cut3 } from './cut.js';
import { clip } from './clip.js';
import { setTag } from './tag.js';
import { withTestAssets } from './test_session_util.js';
import { renderPng } from './renderPng.js';
import { testPng } from './test_png.js';

describe('gap', () => {
  it('join should ignore gaps', async () => {
    await withTestAssets('join should ignore gaps', async (assets) => {
      const box = Box3(assets, [0, 2], [0, 2], [0, 2]);
      const gapTool = setTag(
        Box3(assets, [1, 3], [0, 2], [0, 2]),
        'isGap',
        true
      );
      const result = join(assets, box, [gapTool], 0.01);

      // The result should be identical to the original box because the gap was ignored.
      const image = await renderPng(assets, result, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/gap.test.join_ignored.png`, image)
      );
    });
  });

  it('cut should respect gaps', async () => {
    await withTestAssets('cut should respect gaps', async (assets) => {
      const box = Box3(assets, [0, 2], [0, 2], [0, 2]);
      const gapTool = setTag(
        Box3(assets, [1, 3], [0, 2], [0, 2]),
        'isGap',
        true
      );
      const result = cut3(assets, box, [gapTool]);

      const image = await renderPng(assets, result, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(
          `${import.meta.dirname}/gap.test.cut_respected.png`,
          image
        )
      );
    });
  });

  it('clip should ignore gaps', async () => {
    await withTestAssets('clip should ignore gaps', async (assets) => {
      const box = Box3(assets, [0, 2], [0, 2], [0, 2]);
      const gapTool = setTag(
        Box3(assets, [1, 3], [0, 2], [0, 2]),
        'isGap',
        true
      );
      const result = clip(assets, box, [gapTool]);

      // Since the gap is ignored and there's no other tool,
      // the result of intersection with empty tool should be empty?
      // Wait, Clip unions tools and then intersects target with them.
      // If the union is empty, intersection is empty.

      const image = await renderPng(assets, result, {
        view: { position: [15, 15, 15] },
        width: 512,
        height: 512,
      });
      assert.ok(
        await testPng(`${import.meta.dirname}/gap.test.clip_ignored.png`, image)
      );
    });
  });
});
