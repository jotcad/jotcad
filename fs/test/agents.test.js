import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

test('Agent-style Processors (as Providers)', async (t) => {
  const vfs = new VFS({ id: 'agent-vfs', storage: new MemoryStorage() });
  await vfs.init();

  vfs.registerProvider('compute/sum', async (v, s) => {
    const { a, b } = s.parameters;
    const sum = a + b;
    return { result: sum };
  });

  await t.test('cascading demand works', async () => {
    const selector = new Selector('compute/sum', { a: 10, b: 20 });
    const data = await vfs.readData(selector);
    assert.strictEqual(data?.result, 30);
  });

  await t.test('caching prevents redundant computation', async () => {
    let callCount = 0;
    vfs.registerProvider('compute/once', async () => {
      callCount++;
      return new TextEncoder().encode('done');
    });

    const sel = new Selector('compute/once', { id: 1 });
    await vfs.readData(sel);
    await vfs.readData(sel);

    assert.strictEqual(callCount, 1);
  });

  await vfs.close();
});
