import test from 'node:test';
import assert from 'node:assert';
import { getCID, encodeSafe } from '../src/cid.js';

test('JS CID Reference Values (JCB Binary)', async (t) => {
  await t.test('empty', async () => {
    const val = {};
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    // Hash of the raw JCB binary BgAAAAA=
    assert.strictEqual(cid, 'b45482224b439a3d548c65378929b7dcc16a42288530b7b20d5c8103cc879d10');
    assert.strictEqual(safe, 'BgAAAAA=');
  });

  await t.test('integers', async () => {
    const val = { a: 1, b: 2 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '1ba286df53f7c8304c92cc505d3c6f0eb3b16bd13b66e0e09b2a3bf621b9624c');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA=');
  });

  await t.test('floats', async () => {
    const val = { a: 1.5, b: 2.5 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '3022824d1e31f82c434149d9891deeb765b7c0b8fce8972d961caa5344ddfe5c');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/gAAAAAAAAEAAAAAWIDQAQAAAAAAAA=');
  });

  await t.test('nested', async () => {
    const val = { a: { b: 1 } };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '0bcf69b5300724a671d5b7737c2c66252fadfa0bfa4a0e5c87af70acbb36fc7f');
    assert.strictEqual(safe, 'BgAAAAEEAAAAAWEGAAAAAQQAAAABYgM/8AAAAAAAAA==');
  });

  await t.test('integers as floats', async () => {
    const val = { a: 1.0, b: 2.0 };
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.strictEqual(cid, '1ba286df53f7c8304c92cc505d3c6f0eb3b16bd13b66e0e09b2a3bf621b9624c');
    assert.strictEqual(safe, 'BgAAAAIEAAAAAWEDP/AAAAAAAAAEAAAAAWIDQAAAAAAAAAA=');
  });
});
