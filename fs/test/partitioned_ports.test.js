import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, getSelectorKey } from '../src/index.js';

test('Partitioned Output Ports', async (t) => {
  const vfs = new VFS({ id: 'ports-vfs', storage: new MemoryStorage('ports-vfs') });
  await vfs.init();

  const baseSelector = { path: 'jot/test_op', parameters: { a: 1 } };
  const baseKey = await getSelectorKey(baseSelector);

  await t.test('Writing to partitioned ports', async () => {
    // In the new architecture, every distinct output port hashes to its own unique CID.
    
    await vfs.writeData(baseSelector, { tags: { type: 'shape' }, geometry: 'cid-geom' }); // Primary artifact (no output)
    await vfs.writeData(baseSelector, new TextEncoder().encode('raw bytes data'), { output: 'file' });
    await vfs.writeData(baseSelector, { status: 'success', size: 14 }, { output: 'status' });
    
    // Verify that adding an output CHANGES the CID (Identity is the full Selector)
    const key1 = await getSelectorKey({ ...baseSelector });
    const key2 = await getSelectorKey({ ...baseSelector, output: 'file' });
    assert.notStrictEqual(key1, key2, 'Adding a port must yield a different CID');
  });

  await t.test('Reading from specific ports', async () => {
    // 1. Default read (no output) should return the primary artifact
    const defaultData = await vfs.readData(baseSelector);
    assert.ok(defaultData, 'Default read should return data');
    assert.strictEqual(defaultData.tags?.type, 'shape', 'Primary artifact should be found');

    // 2. Explicit file port (bytes)
    const fileData = await vfs.readData(baseSelector, { output: 'file' });
    assert.ok(fileData instanceof Uint8Array, 'File port should return bytes');
    assert.strictEqual(new TextDecoder().decode(fileData), 'raw bytes data');

    // 3. Explicit status port (json)
    const statusData = await vfs.readData(baseSelector, { output: 'status' });
    assert.strictEqual(statusData.status, 'success');
    assert.strictEqual(statusData.size, 14);
  });

  await t.test('Reading missing ports', async () => {
    const missingData = await vfs.readData(baseSelector, { output: 'nonexistent' });
    assert.strictEqual(missingData, null, 'Missing ports should gracefully return null');
  });

  await vfs.close();
});
