import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { VFS, MemoryStorage } from '../../fs/src/vfs_core.js';
import { registerUGCFileProvider } from '../src/file_provider.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('UGCFileProvider: Resolves sandboxed assets within ugc/models', async () => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();
  registerUGCFileProvider(vfs);

  const fileSel = new Selector('jot/File', { path: 'animals/bear.stl' }).withOutput('$out');
  const result = await vfs.readSelector(fileSel);

  assert.ok(result);
  assert.strictEqual(result.metadata.encoding, 'bytes');
  assert.strictEqual(result.metadata.filename, 'bear.stl');
  
  let chunks = [];
  for await (const chunk of result.stream) {
    chunks.push(chunk);
  }
  const buffer = Buffer.concat(chunks);
  assert.ok(buffer.length > 1000);
});

test('UGCFileProvider: Rejects path traversal outside ugc/models', async () => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();
  registerUGCFileProvider(vfs);

  const illegalSel = new Selector('jot/File', { path: '../../etc/passwd' }).withOutput('$out');
  await assert.rejects(async () => {
    await vfs.readSelector(illegalSel);
  }, /Access denied/);
});
