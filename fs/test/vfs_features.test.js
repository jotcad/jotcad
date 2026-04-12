import test from 'node:test';
import assert from 'node:assert';
import { VFS, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';

const STORAGE = path.resolve('.test_vfs_features');

test('VFS Core Features', async (t) => {
  t.after(async () => {
    await fs.rm(STORAGE, { recursive: true, force: true }).catch(() => {});
  });

  await t.test('writeData() and readData() handle JSON automatically', async () => {
    const vfs = new VFS({ id: 'test-json', storage: new DiskStorage(STORAGE) });
    await vfs.init();
    
    const complexData = { foo: 'bar', nums: [1, 2, 3] };
    await vfs.writeData('data/json', {}, complexData);
    
    const readBack = await vfs.readData('data/json');
    assert.deepStrictEqual(readBack, complexData);
    await vfs.close();
  });

  await t.test('read() deduplication prevents multiple redundant mesh calls', async () => {
    const vfs = new VFS({ id: 'test-dedup' });
    await vfs.init();
    
    let meshCallCount = 0;
    vfs.mesh = {
        read: async (path, params) => {
            meshCallCount++;
            await new Promise(r => setTimeout(r, 100)); // Simulate slow mesh
            await vfs.writeData(path, params, "fulfilled");
            return true; // Indicate work finished
        },
        stop: () => {}
    };

    // Start multiple concurrent reads
    const [s1, s2] = await Promise.all([
        vfs.readText('pending/item'),
        vfs.readText('pending/item')
    ]);
    
    assert.strictEqual(s1, "fulfilled");
    assert.strictEqual(s2, "fulfilled");
    assert.strictEqual(meshCallCount, 1, 'Should only call mesh once for same selector');
    
    await vfs.close();
  });
});
