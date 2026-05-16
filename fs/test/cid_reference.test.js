import test from 'node:test';
import assert from 'node:assert';
import { getCID, encodeSafe, Selector } from '../src/cid.js';

test('JS CID Reference Values (JCB Binary)', async (t) => {
  await t.test('empty', async () => {
    const val = new Selector('test', {});
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.ok(cid);
    assert.ok(safe);
  });
  await t.test('integers', async () => {
    const val = new Selector('test', { a: 1, b: 2 });
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.ok(cid);
    assert.ok(safe);
  });

  await t.test('floats', async () => {
    const val = new Selector('test', { a: 1.5, b: 2.5 });
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.ok(cid);
    assert.ok(safe);
  });

  await t.test('nested', async () => {
    const val = new Selector('test', { a: { b: 1 } });
    const cid = await getCID(val);
    const safe = encodeSafe(val);
    assert.ok(cid);
    assert.ok(safe);
  });
});
