import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

import { Box3 } from './box.js';
import { fromJot, toJot } from './jot.js';
import { makeAbsolute } from './makeAbsolute.js';
import { withTestAssets } from './test_session_util.js';
import {
  DecodeInexactGeometryText,
  EncodeInexactGeometryText,
} from './geometry.js';
import { join } from './join.js';

describe('jot', () => {
  it('should convert a simple Box3 to jot format and back', async (t) => {
    await withTestAssets(
      'should convert a simple Box3 to jot format and back',
      async (assets) => {
        // 1. Create an original shape (Box3)
        const originalShape = Box3(assets, [0, 1], [0, 1], [0, 1]);

        // 2. Convert the original shape to jot format
        const jotId = await toJot(assets, originalShape);
        assert.ok(jotId, 'toJot should return a jot ID');

        // 3. Convert the jot format back to a shape
        const roundTrippedShape = await fromJot(assets, jotId);

        // 4. Compare the original and round-tripped shapes
        const absoluteOriginal = makeAbsolute(assets, originalShape);
        const absoluteRoundTripped = makeAbsolute(assets, roundTrippedShape);

        assert.strictEqual(
          absoluteOriginal.geometry,
          absoluteRoundTripped.geometry,
          'Round-tripped shape geometry should match original'
        );
      }
    );
  });

  it('should handle a more complex shape (joined boxes)', async (t) => {
    await withTestAssets(
      'should handle a more complex shape (joined boxes)',
      async (assets) => {
        // 1. Create a more complex original shape (two joined Box3s)
        const box1 = Box3(assets, [0, 1], [0, 1], [0, 1]);
        const box2 = Box3(assets, [0.5, 1.5], [0.5, 1.5], [0.5, 1.5]);
        const originalShape = join(assets, box1, [box2]);

        // 2. Convert the original shape to jot format
        const jotId = await toJot(assets, originalShape);
        assert.ok(jotId, 'toJot should return a jot ID');

        // 3. Convert the jot format back to a shape
        const roundTrippedShape = await fromJot(assets, jotId);

        // 4. Compare the original and round-tripped shapes
        const absoluteOriginal = makeAbsolute(assets, originalShape);
        const absoluteRoundTripped = makeAbsolute(assets, roundTrippedShape);

        assert.strictEqual(
          absoluteOriginal.geometry,
          absoluteRoundTripped.geometry,
          'Round-tripped complex shape geometry should match original'
        );
      }
    );
  });

  it('should correctly decode and encode simple geometry text', async (t) => {
    await withTestAssets(
      'should correctly decode and encode simple geometry text',
      async (assets) => {
        const simpleGeometryText = `v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 0 1 0 0 1 0
t 0 1 2
`;
        const decoded = DecodeInexactGeometryText(simpleGeometryText);
        const encoded = EncodeInexactGeometryText(decoded);
        assert.strictEqual(
          encoded,
          simpleGeometryText,
          'Encoded text should match original decoded text'
        );
      }
    );
  });
});
