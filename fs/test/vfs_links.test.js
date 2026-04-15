import { test, describe, beforeEach, it } from 'node:test';
import { VFS, MemoryStorage } from '../src/vfs_core.js';
import assert from 'assert';

describe('VFS Link Following (Formal Metadata Only)', () => {
  let vfs;

  beforeEach(() => {
    vfs = new VFS({
      id: 'test-vfs',
      storage: new MemoryStorage()
    });
  });

  // --- FORMAL LINK ENTRIES (The Primary Specification) ---

  it('should follow formal metadata-based Link Entries (target selector)', async () => {
    const targetPath = 'geo/mesh';
    const targetParams = { cid: 'abc123' };
    const targetData = { vertices: [0, 0, 0] };

    // 1. Write the actual target data
    await vfs.writeData(targetPath, targetParams, targetData);

    // 2. Create a formal Link Entry (Aliasing)
    // This creates a CID with metadata.target = {path, parameters}
    await vfs.link('jot/Triangle/geometry', { side: 10 }, targetPath, targetParams);

    // 3. Reading the source should return the target data transparently
    const result = await vfs.readData('jot/Triangle/geometry', { side: 10 });
    assert.deepStrictEqual(result, targetData);
  });

  it('should follow formal links even if data is already in storage', async () => {
    const targetPath = 'real/data';
    const targetData = "I am the real data";
    await vfs.writeData(targetPath, {}, targetData);

    // Create a formal link
    await vfs.link('alias/path', {}, targetPath, {});

    // Now reading 'alias/path' should resolve to 'real/data'
    const result = await vfs.readText('alias/path', {});
    assert.strictEqual(result, targetData);
  });

  it('should NOT follow text-based vfs:/ strings in raw data (No Guessing)', async () => {
    // A provider returns a raw text string that happens to look like a link
    vfs.registerProvider('fake/link', async () => {
      return 'vfs:/should/not/be/followed';
    });

    const result = await vfs.readText('fake/link', {});
    assert.strictEqual(result, 'vfs:/should/not/be/followed');
  });

  it('should NOT follow JSON-based selector strings in raw data (No Guessing)', async () => {
    // A provider returns a JSON object that looks like a selector
    const selectorLike = {
      path: 'some/path',
      parameters: { val: 42 }
    };

    vfs.registerProvider('fake/json-link', async () => {
      return JSON.stringify(selectorLike);
    });

    const result = await vfs.readData('fake/json-link', {});
    // Should return the object itself, not attempt to "follow" it
    assert.deepStrictEqual(result, selectorLike);
  });

  it('should NOT follow JSON objects that are actual shapes (have geometry)', async () => {
    const shape = {
      geometry: { path: 'geo/mesh', parameters: { cid: '123' } },
      parameters: { size: 10 }
    };

    vfs.registerProvider('shape/real', async () => {
      return JSON.stringify(shape);
    });

    const result = await vfs.readData('shape/real', {});
    // Should return the shape itself
    assert.deepStrictEqual(result, shape);
  });

  it('should respect followLinks: false context for formal links', async () => {
    await vfs.writeData('target', {}, "DATA");
    await vfs.link('source', {}, 'target', {});

    const result = await vfs.readData('source', {}, { followLinks: false });
    // Should return the raw bytes of the source CID (which has a dummy payload)
    const text = new TextDecoder().decode(result);
    assert.ok(text.startsWith('vfs:/target'));
  });
});
