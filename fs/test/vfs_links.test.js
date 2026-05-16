import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

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

async function consumeText(stream) {
    const bytes = await consumeBytes(stream);
    return new TextDecoder().decode(bytes);
}

test('VFS Link Following (Formal Metadata Only)', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  await t.test('should follow formal metadata-based Link Entries (target selector)', async () => {
    const target = new Selector('real/data', { id: 123 });
    const alias = new Selector('link/to/data');
    const payload = { vertices: [0, 0, 0] };

    await vfs.write(target, payload, { encoding: 'json' });
    await vfs.link(alias, target);

    const { stream, metadata } = await vfs.read(alias);
    assert.strictEqual(metadata.encoding, 'json');
    const result = await consumeJSON(stream);
    assert.deepStrictEqual(result, payload);
  });

  await t.test('should follow formal links even if data is already in storage', async () => {
    const target = new Selector('target', { id: 1 });
    const alias = new Selector('alias', { id: 1 });
    
    await vfs.write(target, 'I am the real data', { encoding: 'string' });
    
    // Create a Link Entry for 'alias'
    await vfs.link(alias, target);
    
    const { stream } = await vfs.read(alias);
    const result = await consumeText(stream);
    assert.strictEqual(result, 'I am the real data');
  });

  await t.test('should NOT follow text-based vfs:/ strings in raw data (No Guessing)', async () => {
    const target = new Selector('secret');
    const bait = new Selector('bait');
    
    await vfs.write(target, 'top-secret', { encoding: 'string' });
    await vfs.write(bait, 'vfs:/secret', { encoding: 'string' }); // Just text
    
    const { stream } = await vfs.read(bait);
    const result = await consumeText(stream);
    assert.strictEqual(result, 'vfs:/secret');
  });

  await t.test('should NOT follow JSON-based selector strings in raw data (No Guessing)', async () => {
    const target = new Selector('secret');
    const bait = new Selector('bait');
    
    await vfs.write(target, 'top-secret', { encoding: 'string' });
    const targetJSON = JSON.stringify(target.toJSON());
    await vfs.write(bait, targetJSON, { encoding: 'string' }); // Just JSON string
    
    const { stream } = await vfs.read(bait);
    const result = await consumeText(stream);
    // result remains a string, even if it looks like JSON
    assert.strictEqual(result, targetJSON);
  });

  await t.test('should NOT follow JSON objects that are actual shapes (have geometry)', async () => {
    const target = new Selector('secret');
    const shape = new Selector('shape');
    
    await vfs.write(target, 'top-secret', { encoding: 'string' });
    await vfs.write(shape, { geometry: 'vfs:/secret', tags: { type: 'bait' } }, { encoding: 'json' });
    
    const { stream } = await vfs.read(shape);
    const result = await consumeJSON(stream);
    assert.strictEqual(result.tags.type, 'bait');
    assert.strictEqual(result.geometry, 'vfs:/secret');
  });

  await t.test('should respect followLinks: false context for formal links', async () => {
    const target = new Selector('target');
    const alias = new Selector('alias');
    await vfs.write(target, 'real', { encoding: 'string' });
    await vfs.link(alias, target);
    
    const { stream, metadata } = await vfs.read(alias, { followLinks: false });
    assert.strictEqual(metadata.encoding, 'link');
    const result = await consumeJSON(stream);
    assert.deepStrictEqual(result, target.toJSON());
  });

  await vfs.close();
});
