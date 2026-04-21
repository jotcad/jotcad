import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, getSelectorKey } from '../src/index.js';

test('Partitioned Output Ports', async (t) => {
  const vfs = new VFS({ id: 'ports-vfs', storage: new MemoryStorage('ports-vfs') });
  await vfs.init();

  const baseSelector = { path: 'jot/test_op', parameters: { a: 1 } };
  const baseKey = await getSelectorKey(baseSelector);

  await t.test('Writing to partitioned ports', async () => {
    // Write multiple outputs for a single computation
    // We expect the VFS write API to handle an outputs map, or we can write them sequentially
    // Let's assume writeData can take an explicit `output` in context, and groups them under the same base selector.
    
    await vfs.writeData(baseSelector, { tags: { type: 'shape' }, geometry: 'cid-geom' }, { output: '$out' });
    await vfs.writeData(baseSelector, new TextEncoder().encode('raw bytes data'), { output: 'file', type: 'bytes' });
    await vfs.writeData(baseSelector, { status: 'success', size: 14 }, { output: 'status' });
    
    // Verify all writes share the same Base Selector Identity
    const key1 = await getSelectorKey({ ...baseSelector });
    assert.strictEqual(key1, baseKey, 'Identity must remain stable and bound to the base selector');
  });

  await t.test('Reading from specific ports', async () => {
    // 1. Default port should be $out
    const defaultData = await vfs.readData(baseSelector);
    assert.ok(defaultData, 'Default read should return data');
    assert.strictEqual(defaultData.tags?.type, 'shape', 'Default port should be $out');

    // 2. Explicit $out port
    const outData = await vfs.readData(baseSelector, { output: '$out' });
    assert.deepStrictEqual(outData, defaultData, 'Explicit $out should match default');

    // 3. Explicit file port (bytes)
    const fileData = await vfs.readData(baseSelector, { output: 'file' });
    assert.ok(fileData instanceof Uint8Array, 'File port should return bytes');
    assert.strictEqual(new TextDecoder().decode(fileData), 'raw bytes data');

    // 4. Explicit status port (json)
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
