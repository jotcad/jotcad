import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

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
      return new TextEncoder().encode('fulfilled');
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

  await vfs.close();
});
