import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, DiskStorage, Selector } from '../src/index.js';
import { Readable } from 'stream';
import fs from 'fs';
import path from 'path';
import os from 'os';

async function consumeText(stream) {
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
    return new TextDecoder().decode(bytes);
}

test('VFS DiskStorage and Sessions', async (t) => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'vfs-test-'));

  t.after(() => {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  });

  await t.test('DiskStorage offloads to disk', async () => {
    const root = path.join(tmpDir, 'session-1');
    const storage = new DiskStorage(root);
    const vfs = new VFS({ storage });
    await vfs.init();

    const content = 'large-data-on-disk';
    await vfs.write(new Selector('big-file'), content, { encoding: 'string' });

    // Verify files exist on disk (.meta and .data)
    const files = fs.readdirSync(root);
    assert.ok(files.length >= 2, 'Should have at least meta and data files');

    // Read it back
    const { stream, metadata } = await vfs.read(new Selector('big-file'));
    assert.strictEqual(metadata.encoding, 'string');
    const result = await consumeText(stream);
    assert.strictEqual(result, content);

    await vfs.close();
  });

  await t.test('Session isolation via DiskStorage', async () => {
    const rootA = path.join(tmpDir, 'session-A');
    const rootB = path.join(tmpDir, 'session-B');

    const vfsA = new VFS({ id: 'A', storage: new DiskStorage(rootA) });
    const vfsB = new VFS({ id: 'B', storage: new DiskStorage(rootB) });
    await vfsA.init();
    await vfsB.init();

    await vfsA.write(new Selector('shared-path'), 'data-A', { encoding: 'string' });
    await vfsB.write(new Selector('shared-path'), 'data-B', { encoding: 'string' });

    const { stream: streamA } = await vfsA.read(new Selector('shared-path'));
    const resA = await consumeText(streamA);
    assert.strictEqual(resA, 'data-A');

    const { stream: streamB } = await vfsB.read(new Selector('shared-path'));
    const resB = await consumeText(streamB);
    assert.strictEqual(resB, 'data-B');

    await vfsA.close();
    await vfsB.close();
  });
});
