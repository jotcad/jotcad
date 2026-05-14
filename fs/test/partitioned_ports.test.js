import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, getSelectorKey, Selector } from '../src/index.js';

async function consumeBytes(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return bytes;
}

async function consumeJSON(stream) {
    const bytes = await consumeBytes(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('Partitioned Output Ports', async (t) => {
  const vfs = new VFS({ id: 'ports-vfs', storage: new MemoryStorage('ports-vfs') });
  await vfs.init();

  const baseSelector = new Selector('jot/test_op', { a: 1 });
  const baseKey = await getSelectorKey(baseSelector);

  await t.test('Writing to partitioned ports', async () => {
    // In the new architecture, every distinct output port hashes to its own unique CID.
    
    await vfs.write(baseSelector, { tags: { type: 'shape' }, geometry: 'cid-geom' }, { encoding: 'json' }); 
    await vfs.write(baseSelector.withOutput('file'), new TextEncoder().encode('raw bytes data'), { encoding: 'bytes' });
    await vfs.write(baseSelector.withOutput('status'), { status: 'success', size: 14 }, { encoding: 'json' });
    
    // Verify that adding an output CHANGES the CID (Identity is the full Selector)
    const key1 = await getSelectorKey(baseSelector);
    const key2 = await getSelectorKey(baseSelector.withOutput('file'));
    assert.notStrictEqual(key1, key2, 'Adding a port must yield a different CID');
  });

  await t.test('Reading from specific ports', async () => {
    // 1. Default read (no output) should return the primary artifact
    const { stream: stream1, metadata: meta1 } = await vfs.read(baseSelector);
    assert.strictEqual(meta1.encoding, 'json');
    const defaultData = await consumeJSON(stream1);
    assert.ok(defaultData, 'Default read should return data');
    assert.strictEqual(defaultData.tags?.type, 'shape', 'Primary artifact should be found');

    // 2. Explicit file port (bytes)
    const { stream: stream2, metadata: meta2 } = await vfs.read(baseSelector.withOutput('file'));
    assert.strictEqual(meta2.encoding, 'bytes');
    const fileData = await consumeBytes(stream2);
    assert.ok(fileData instanceof Uint8Array, 'File port should return bytes');
    assert.strictEqual(new TextDecoder().decode(fileData), 'raw bytes data');

    // 3. Explicit status port (json)
    const { stream: stream3, metadata: meta3 } = await vfs.read(baseSelector.withOutput('status'));
    assert.strictEqual(meta3.encoding, 'json');
    const statusData = await consumeJSON(stream3);
    assert.strictEqual(statusData.status, 'success');
    assert.strictEqual(statusData.size, 14);
  });

  await t.test('Reading missing ports', async () => {
    const res = await vfs.read(baseSelector.withOutput('nonexistent'));
    assert.strictEqual(res, null, 'Missing ports should gracefully return null');
  });

  await vfs.close();
});
