import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';

test('VFS Link Resolution & Metadata Aggregation', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });

  await t.test('should transparently follow multi-hop links', async () => {
    const s1 = { path: 'link/start', parameters: {} };
    const s2 = { path: 'link/intermediate', parameters: {} };
    const s3 = { path: 'link/terminal', parameters: {} };
    
    await vfs.writeData(s3, 'TERMINAL');
    await vfs.link(s2, s3);
    await vfs.link(s1, s2);
    
    const result = await vfs.readText(s1);
    assert.strictEqual(result, 'TERMINAL');
  });

  await t.test('should aggregate metadata along the chain', async () => {
    const s1 = { path: 'link/start', parameters: {} };
    const s2 = { path: 'link/terminal', parameters: {} };
    
    await vfs.writeData(s2, { tags: { owner: 'brian' } });
    await vfs.link(s1, s2);
    
    const result = await vfs.readData(s1);
    assert.strictEqual(result.tags.owner, 'brian');
  });

  await t.test('should handle circular links gracefully', async () => {
    const s1 = { path: 'circle/a', parameters: {} };
    const s2 = { path: 'circle/b', parameters: {} };
    
    await vfs.link(s1, s2);
    await vfs.link(s2, s1);
    
    try {
      await vfs.read(s1);
      assert.fail('Should have thrown cycle error');
    } catch (err) {
      assert.ok(err.message.includes('Circular link detected'));
    }
  });

  await t.test('should return non-link JSON as terminal data', async () => {
    const s1 = { path: 'data/json', parameters: {} };
    const data = { some: 'value' };
    await vfs.writeData(s1, data);
    
    const result = await vfs.readData(s1);
    assert.deepStrictEqual(result, data);
  });

  await vfs.close();
});
