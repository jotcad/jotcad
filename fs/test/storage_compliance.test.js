import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs/promises';
import path from 'node:path';
import { MemoryStorage } from '../src/vfs_core.js';
import { DiskStorage } from '../src/vfs_node.js';
import { IndexedDBStorage } from '../src/vfs_browser.js';

/**
 * Standard Storage Compliance Test Suite
 * Runs against any implementation of the VFS Storage interface.
 */
async function runComplianceSuite(t, storage, cleanup) {
  const cid = 'test-cid-' + Math.random().toString(36).slice(2);
  const data = new TextEncoder().encode('Hello JotCAD');
  const info = { path: 'test/path', size: data.length };

  await t.test('should store and retrieve Uint8Array', async () => {
    await storage.set(cid, data, info);
    const stream = await storage.get(cid);
    assert.ok(stream !== null);
    const reader = stream.getReader();
    const { value } = await reader.read();
    assert.deepStrictEqual(value, data);
  });

  await t.test('should store and retrieve string', async () => {
    const strCid = cid + '-str';
    await storage.set(strCid, 'string-data', { ...info, size: 11 });
    const stream = await storage.get(strCid);
    const reader = stream.getReader();
    const { value } = await reader.read();
    assert.strictEqual(new TextDecoder().decode(value), 'string-data');
  });

  await t.test(
    'should enforce size verification (Aborted Stream)',
    async () => {
      const badCid = cid + '-bad';
      const stream = new ReadableStream({
        start(c) {
          c.enqueue(new Uint8Array([1, 2, 3]));
          c.close();
        },
      });

      try {
        await storage.set(badCid, stream, { ...info, size: 100 });
        assert.fail('Should have thrown size mismatch error');
      } catch (err) {
        assert.ok(
          err.message.includes('Expected 100') ||
            err.message.includes('aborted')
        );
      }

      // Ensure it wasn't cached
      const exists = await storage.has(badCid);
      assert.strictEqual(
        exists,
        false,
        'Corrupted data should not be accessible'
      );
    }
  );

  if (cleanup) await cleanup();
}

test('Storage Compliance: MemoryStorage', async (t) => {
  const storage = new MemoryStorage();
  await runComplianceSuite(t, storage);
});

test('Storage Compliance: DiskStorage', async (t) => {
  const root = '.test_storage_disk';
  const storage = new DiskStorage(root);
  await runComplianceSuite(t, storage, async () => {
    await fs.rm(root, { recursive: true, force: true });
  });
});

test('Storage Compliance: IndexedDBStorage (Mocked)', async (t) => {
  // Mock the global indexedDB environment for the test
  const mockDB = {
    transaction: () => ({
      objectStore: () => ({
        put: (val, key) => {
          const req = { onsuccess: null, onerror: null };
          setTimeout(() => {
            mockDB._store.set(key, val);
            if (req.onsuccess) req.onsuccess();
          }, 0);
          return req;
        },
        get: (key) => {
          const req = { onsuccess: null, onerror: null };
          setTimeout(() => {
            req.result = mockDB._store.get(key);
            if (req.onsuccess) req.onsuccess();
          }, 0);
          return req;
        },
        count: (key) => {
          const req = { onsuccess: null, onerror: null };
          setTimeout(() => {
            req.result = mockDB._store.has(key) ? 1 : 0;
            if (req.onsuccess) req.onsuccess();
          }, 0);
          return req;
        },
      }),
    }),
    _store: new Map(),
  };

  // Inject mock into storage instance
  const storage = new IndexedDBStorage('test-db');
  storage._getDB = async () => mockDB;

  await runComplianceSuite(t, storage);
});
