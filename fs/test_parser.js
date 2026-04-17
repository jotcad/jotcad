import test from 'node:test';
import assert from 'node:assert';
import { VFS, WebReadableStream } from './src/vfs_core.js';

test('parseVFSBundle', async () => {
  const chunk1 = VFS.formatVFSChunk(
    { path: 'a' },
    new TextEncoder().encode('hello')
  );
  const chunk2 = VFS.formatVFSChunk(
    { path: 'b' },
    new TextEncoder().encode('world')
  );

  const combined = new Uint8Array(chunk1.length + chunk2.length);
  combined.set(chunk1);
  combined.set(chunk2, chunk1.length);

  const stream = new WebReadableStream({
    start(c) {
      c.enqueue(combined);
      c.close();
    },
  });

  const results = [];
  for await (const item of VFS.parseVFSBundle(stream)) {
    results.push({
      selector: item.selector,
      text: new TextDecoder().decode(item.data),
    });
  }

  assert.strictEqual(results.length, 2);
  assert.strictEqual(results[0].selector.path, 'a');
  assert.strictEqual(results[0].text, 'hello');
  assert.strictEqual(results[1].selector.path, 'b');
  assert.strictEqual(results[1].text, 'world');
});
