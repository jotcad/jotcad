import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';
import { vfsResult } from './vfs_test_helpers.js';

async function consumeStream(stream) {
    const chunks = [];
    const reader = stream.getReader();
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

test('VFS Core Features (Pure Router)', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  await t.test('write() and read() handle raw data and metadata', async () => {
    const selector = new Selector('test/json', { id: 1 });
    const data = { foo: 'bar', nums: [1, 2, 3] };

    // Caller explicitly defines encoding
    await vfs.write(selector, data, { encoding: 'json' });
    
    // Caller explicitly consumes and decodes
    const { stream, metadata } = await vfs.read(selector);
    assert.strictEqual(metadata.encoding, 'json');
    
    const bytes = await consumeStream(stream);
    const result = JSON.parse(new TextDecoder().decode(bytes));
    assert.deepStrictEqual(result, data);
  });

  await t.test('read() deduplication prevents multiple redundant mesh calls', async () => {
    let callCount = 0;
    vfs.registerProvider('test/dedup', async () => {
      callCount++;
      await new Promise(r => setTimeout(r, 50));
      return vfsResult('fulfilled');
    });

    const sel = new Selector('test/dedup', { id: 1 });
    
    // Multiple concurrent reads for the same target
    const [r1, r2] = await Promise.all([
      vfs.read(sel),
      vfs.read(sel)
    ]);

    const bytes1 = await consumeStream(r1.stream);
    const bytes2 = await consumeStream(r2.stream);

    assert.strictEqual(new TextDecoder().decode(bytes1), 'fulfilled');
    assert.strictEqual(new TextDecoder().decode(bytes2), 'fulfilled');
    assert.strictEqual(callCount, 1, 'Provider should only be called once');
  });

  await t.test('path-oriented APIs: write(), read(), listen(), and unlisten()', async () => {
    const path = 'test/path/data';
    const payload = { hello: 'world', values: [10, 20] };

    // 1. Test listen
    let received = null;
    let listenCallCount = 0;
    await vfs.listen(path, (val) => {
      received = val;
      listenCallCount++;
    });

    // 2. Test write
    await vfs.write(path, payload);
    await new Promise(resolve => setTimeout(resolve, 10));

    // 3. Test listener received notified payload
    assert.deepStrictEqual(received, payload);
    assert.strictEqual(listenCallCount, 1);

    // 4. Test read of path string
    const readVal = await vfs.read(path);
    assert.deepStrictEqual(readVal, payload);

    // 5. Test unlisten
    await vfs.unlisten(path);
    await vfs.write(path, { hello: 'changed' });
    await new Promise(resolve => setTimeout(resolve, 10));

    assert.strictEqual(listenCallCount, 1);
    assert.deepStrictEqual(received, payload);
  });

  await t.test('path-oriented APIs: fulfill() triggers provider without returning payload data', async () => {
    let callCount = 0;
    vfs.registerProvider('test/fulfill-trigger', async () => {
      callCount++;
      return vfsResult({ triggered: true });
    });

    const path = 'test/fulfill-trigger';
    
    // Call fulfill
    const res = await vfs.fulfill(path);
    
    // It should return the metadata (status, encoding, etc.)
    assert.strictEqual(res.state, 'AVAILABLE');
    assert.strictEqual(res.encoding, 'json');
    assert.strictEqual(callCount, 1);

    // Verify it cached the data in the VFS storage!
    const cachedResult = await vfs.read(path);
    assert.deepStrictEqual(cachedResult, { triggered: true });
    assert.strictEqual(callCount, 1); // should not invoke provider again
  });

  await vfs.close();
});
