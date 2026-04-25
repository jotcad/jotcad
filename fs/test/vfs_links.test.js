import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

test('VFS Link Following (Formal Metadata Only)', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  await t.test('should follow formal metadata-based Link Entries (target selector)', async () => {
    const target = new Selector('real/data', { id: 123 });
    const alias = new Selector('link/to/data');
    const payload = { vertices: [0, 0, 0] };

    await vfs.writeData(target, payload);
    await vfs.link(alias, target);

    const result = await vfs.readData(alias);
    assert.deepStrictEqual(result, payload);
  });

  await t.test('should follow formal links even if data is already in storage', async () => {
    const target = new Selector('target', { id: 1 });
    const alias = new Selector('alias', { id: 1 });
    
    await vfs.writeData(target, 'I am the real data');
    
    // Create a Link Entry for 'alias'
    await vfs.link(alias, target);
    
    const result = await vfs.readText(alias);
    assert.strictEqual(result, 'I am the real data');
  });

  await t.test('should NOT follow text-based vfs:/ strings in raw data (No Guessing)', async () => {
    const target = new Selector('secret');
    const bait = new Selector('bait');
    
    await vfs.writeData(target, 'top-secret');
    await vfs.writeData(bait, 'vfs:/secret'); // Just text
    
    const result = await vfs.readText(bait);
    assert.strictEqual(result, 'vfs:/secret');
  });

  await t.test('should NOT follow JSON-based selector strings in raw data (No Guessing)', async () => {
    const target = new Selector('secret');
    const bait = new Selector('bait');
    
    await vfs.writeData(target, 'top-secret');
    await vfs.writeData(bait, JSON.stringify(target)); // Just JSON
    
    const result = await vfs.readText(bait);
    assert.strictEqual(result, JSON.stringify(target));
  });

  await t.test('should NOT follow JSON objects that are actual shapes (have geometry)', async () => {
    const target = new Selector('secret');
    const shape = new Selector('shape');
    
    await vfs.writeData(target, 'top-secret');
    await vfs.writeData(shape, { geometry: 'vfs:/secret', tags: { type: 'bait' } });
    
    const result = await vfs.readData(shape);
    assert.strictEqual(result.tags.type, 'bait');
    assert.strictEqual(result.geometry, 'vfs:/secret');
  });

  await t.test('should respect followLinks: false context for formal links', async () => {
    const target = new Selector('target');
    const alias = new Selector('alias');
    await vfs.writeData(target, 'real');
    await vfs.link(alias, target);
    
    const result = await vfs.read(alias, { followLinks: false });
    assert.ok(result, 'Result should not be null even with followLinks: false (should return link text)');
    const reader = result.getReader();
    const { value } = await reader.read();
    const text = new TextDecoder().decode(value);
    assert.ok(text.startsWith('vfs:/target'));
  });

  await vfs.close();
});
