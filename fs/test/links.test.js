import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

async function consumeJSON(stream) {
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
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('VFS Symmetric Links (Pure Router)', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  await t.test('Link resolution follows to terminal data', async () => {
    const s1 = new Selector('alias/1');
    const s2 = new Selector('alias/2');
    const s3 = new Selector('data/terminal');

    // Terminal data
    await vfs.write(s3, { msg: 'TERMINAL' }, { encoding: 'json' });

    // s1 -> s2 -> s3
    await vfs.link(s2, s3);
    await vfs.link(s1, s2);

    // Resolving s1 should return s3's data
    const { stream, metadata } = await vfs.read(s1);
    assert.strictEqual(metadata.encoding, 'json');
    
    const data = await consumeJSON(stream);
    assert.deepStrictEqual(data, { msg: 'TERMINAL' });
  });

  await t.test('Metadata Purity in Links', async () => {
    const src = new Selector('link/src');
    const tgt = new Selector('link/tgt');

    await vfs.link(src, tgt);
    const { stream, metadata } = await vfs.read(src, { followLinks: false });
    
    // Link itself should have 'encoding: link'
    assert.strictEqual(metadata.encoding, 'link');
    assert.strictEqual(metadata.state, 'AVAILABLE');
    assert.deepStrictEqual(metadata.selector, src.toJSON());

    // Link data should be the target selector
    const data = await consumeJSON(stream);
    assert.deepStrictEqual(data, tgt.toJSON());
  });

  await t.test('Circular link detection', async () => {
    const s1 = new Selector('cycle/1');
    const s2 = new Selector('cycle/2');

    await vfs.link(s1, s2);
    await vfs.link(s2, s1);

    await assert.rejects(
      vfs.read(s1),
      /Circular link detected/
    );
  });

  await vfs.close();
});
