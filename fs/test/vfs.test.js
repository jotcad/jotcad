import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs.js';
import { Readable } from 'stream';

test('VFS basic lifecycle', async (t) => {
  const vfs = new VFS();

  await t.test('tickle creates a pending request', async () => {
    const status = await vfs.tickle('test/path');
    assert.strictEqual(status, 'PENDING');
  });

  await t.test('lease acquires a provisioning lock', async () => {
    const hasLease = await vfs.lease('test/path', 1000);
    assert.strictEqual(hasLease, true);
    const status = await vfs.status('test/path');
    assert.strictEqual(status, 'PROVISIONING');
  });

  await t.test('write resolves pending reads', async () => {
    const readPromise = vfs.read('test/path');

    const stream = Readable.from(['hello world']);
    await vfs.write('test/path', stream);

    const resultStream = await readPromise;
    assert.ok(resultStream);

    let content = '';
    for await (const chunk of resultStream) {
      content += chunk;
    }
    assert.strictEqual(content, 'hello world');

    const status = await vfs.status('test/path');
    assert.strictEqual(status, 'AVAILABLE');
  });

  await t.test('isolation between VFS instances', async () => {
    const sessionA = new VFS();
    const sessionB = new VFS();

    await sessionA.tickle('test');
    assert.strictEqual(await sessionA.status('test'), 'PENDING');
    assert.strictEqual(await sessionB.status('test'), 'MISSING');
  });

  await t.test('cooperation between coupled VFS instances', async () => {
    const vfsBrowser = new VFS();
    const vfsNode = new VFS();
    vfsBrowser.connect(vfsNode);

    // Browser wants a box
    const readPromise = vfsBrowser.read('box');

    // Node sees the pending request and fulfills it
    const statusOnNode = await vfsNode.status('box');
    assert.strictEqual(statusOnNode, 'PENDING');

    await vfsNode.write('box', Readable.from(['box-data']));

    // Browser should resolve the read
    const stream = await readPromise;
    let content = '';
    for await (const chunk of stream) {
      content += chunk;
    }
    assert.strictEqual(content, 'box-data');
  });
});
