import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, DiskStorage } from '../src/vfs.js';
import { Readable } from 'stream';
import fs from 'fs';
import path from 'path';
import os from 'os';

test('VFS DiskStorage and Sessions', async (t) => {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'vfs-test-'));

  await t.test('DiskStorage offloads to disk', async () => {
    const root = path.join(tmpDir, 'session-1');
    const storage = new DiskStorage(root);
    const vfs = new VFS({ storage });

    const content = 'large-data-on-disk';
    await vfs.write('big-file', Readable.from([content]));

    // Verify files exist on disk (.meta and .data)
    const files = fs.readdirSync(root);
    assert.strictEqual(files.length, 2);
    
    // Read it back
    const stream = await vfs.read('big-file');
    let result = '';
    for await (const chunk of stream) result += chunk;
    assert.strictEqual(result, content);

    await vfs.close();
  });

  await t.test('Session isolation via DiskStorage', async () => {
    const rootA = path.join(tmpDir, 'session-A');
    const rootB = path.join(tmpDir, 'session-B');
    
    const vfsA = new VFS({ storage: new DiskStorage(rootA) });
    const vfsB = new VFS({ storage: new DiskStorage(rootB) });

    await vfsA.write('shared-path', Readable.from(['data-A']));
    await vfsB.write('shared-path', Readable.from(['data-B']));

    const streamA = await vfsA.read('shared-path');
    let resA = '';
    for await (const chunk of streamA) resA += chunk;
    assert.strictEqual(resA, 'data-A');

    const streamB = await vfsB.read('shared-path');
    let resB = '';
    for await (const chunk of streamB) resB += chunk;
    assert.strictEqual(resB, 'data-B');

    await vfsA.close();
    await vfsB.close();
  });

  // Cleanup
  t.after(() => {
    fs.rmSync(tmpDir, { recursive: true, force: true });
  });
});
