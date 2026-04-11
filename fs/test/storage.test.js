import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, DiskStorage } from '../src/index.js';
import { Readable } from 'stream';
import fs from 'fs';
import path from 'path';
import os from 'os';

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
    await vfs.writeData('big-file', {}, content);

    // Verify files exist on disk (.meta and .data)
    const files = fs.readdirSync(root);
    assert.ok(files.length >= 2, 'Should have at least meta and data files');
    
    // Read it back
    const result = await vfs.readData('big-file');
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

    await vfsA.writeData('shared-path', {}, 'data-A');
    await vfsB.writeData('shared-path', {}, 'data-B');

    const resA = await vfsA.readData('shared-path');
    assert.strictEqual(resA, 'data-A');

    const resB = await vfsB.readData('shared-path');
    assert.strictEqual(resB, 'data-B');

    await vfsA.close();
    await vfsB.close();
  });
});
