import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';

test('VFS Core Features', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });

  await t.test('writeData() and readData() handle JSON automatically', async () => {
    const selector = { path: 'test/json', parameters: { id: 1 } };
    const data = { foo: 'bar', nums: [1, 2, 3] };

    await vfs.writeData(selector, data);
    const result = await vfs.readData(selector);
    assert.deepStrictEqual(result, data);
  });

  await t.test('read() deduplication prevents multiple redundant mesh calls', async () => {
    let callCount = 0;
    vfs.registerProvider('test/dedup', async () => {
      callCount++;
      await new Promise(r => setTimeout(r, 50));
      return new TextEncoder().encode('fulfilled');
    });

    const sel = { path: 'test/dedup', parameters: { id: 1 } };
    const [r1, r2] = await Promise.all([
      vfs.readText(sel),
      vfs.readText(sel)
    ]);

    assert.strictEqual(r1, 'fulfilled');
    assert.strictEqual(r2, 'fulfilled');
    assert.strictEqual(callCount, 1, 'Provider should only be called once');
  });

  await vfs.close();
});
