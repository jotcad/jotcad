import test from 'node:test';
import assert from 'node:assert';
import { getCID, encodeSafe } from '../src/cid.js';

test('JS CID Reference Values (Safe-JCB/Base64)', async (t) => {
  await t.test('empty', async () => {
    const val = {};
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '49c5feb68d8bef00a7634384c15d91911981d4e65d01687b169b95e28a7ebcc5');
    assert.strictEqual(safe, 'BgAAAAA=');
  });

  await t.test('integers', async () => {
    const val = { a: 1, b: 2 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, 'd3d36ade80394108d6a7464314c163c2145cf147968b30920efabd51ea688977');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA=');
  });

  await t.test('floats', async () => {
    const val = { a: 1.5, b: 2.5 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '4d74c61eceea590c5348f7dcff288f805665c66a97c226714cb4beb3ad300787');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/gAAAAAAAAEAAAAAWIDQAQAAAAAAAA=');
  });

  await t.test('nested', async () => {
    const val = { a: { b: 1 } };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '4bae03292e31ff8318ffe356e4ca9797049cc919072f7d2d11ea722539d69206');
    assert.strictEqual(safe, 'BgAAAAEEAAAAAWEGAAAAAQQAAAABYgM/8AAAAAAAAA==');
  });

  await t.test('integers as floats', async () => {
    const val = { a: 1.0, b: 2.0 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, 'd3d36ade80394108d6a7464314c163c2145cf147968b30920efabd51ea688977');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA=');
  });
});
