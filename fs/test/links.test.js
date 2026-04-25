import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

test('VFS Link Resolution', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  await t.test('should transparently follow multi-hop links', async () => {
    const s1 = new Selector('link/start');
    const s2 = new Selector('link/intermediate');
    const s3 = new Selector('link/terminal');
    
    await vfs.writeData(s3, 'TERMINAL');
    await vfs.link(s2, s3);
    await vfs.link(s1, s2);
    
    const result = await vfs.readText(s1);
    assert.strictEqual(result, 'TERMINAL');
  });

  await t.test('should handle circular links gracefully', async () => {
    const s1 = new Selector('circle/a');
    const s2 = new Selector('circle/b');
    
    await vfs.link(s1, s2);
    await vfs.link(s2, s1);
    
    try {
      await vfs.read(new Selector('circle/a'));
      assert.fail('Should have thrown cycle error');
    } catch (err) {
      assert.ok(err.message.includes('Circular link detected'));
    }
  });

  await t.test('should return non-link JSON as terminal data', async () => {
    const s1 = new Selector('data/json');
    const data = { some: 'value' };
    await vfs.writeData(s1, data);

    const result = await vfs.readData(new Selector('data/json'));
    assert.deepStrictEqual(result, data);
  });

  await vfs.close();
});
