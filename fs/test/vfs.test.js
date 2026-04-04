import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Readable } from 'stream';

test('VFS basic lifecycle', async (t) => {
  await t.test('tickle creates a pending request', async () => {
    const vfs = new VFS();
    const status = await vfs.tickle('test/path');
    assert.strictEqual(status, 'PENDING');
    await vfs.close();
  });

  await t.test('lease acquires a provisioning lock', async () => {
    const vfs = new VFS();
    await vfs.tickle('test/path');
    const hasLease = await vfs.lease('test/path', 1000);
    assert.strictEqual(hasLease, true);
    const status = await vfs.status('test/path');
    assert.strictEqual(status, 'PROVISIONING');
    await vfs.close();
  });

  await t.test('write resolves pending reads', async () => {
    const vfs = new VFS();
    const readPromise = vfs.read('test/path');

    const stream = Readable.from([Buffer.from('hello world')]);
    await vfs.write('test/path', stream);

    const resultStream = await readPromise;
    assert.ok(resultStream);

    let content = '';
    const decoder = new TextDecoder();
    for await (const chunk of resultStream) {
      content += decoder.decode(chunk, { stream: true });
    }
    content += decoder.decode();
    assert.strictEqual(content, 'hello world');

    const status = await vfs.status('test/path');
    assert.strictEqual(status, 'AVAILABLE');
    await vfs.close();
  });

  await t.test('isolation between VFS instances', async () => {
    const sessionA = new VFS();
    const sessionB = new VFS();

    await sessionA.tickle('test');
    assert.strictEqual(await sessionA.status('test'), 'PENDING');
    assert.strictEqual(await sessionB.status('test'), 'MISSING');
    await sessionA.close();
    await sessionB.close();
  });

  await t.test('cooperation between coupled VFS instances', async () => {
    const vfsBrowser = new VFS();
    const vfsNode = new VFS();
    vfsBrowser.connect(vfsNode);

    // Browser wants a box
    const readPromise = vfsBrowser.read('box');

    // Give state propagation a tick
    await new Promise(r => setTimeout(r, 10));

    // Node sees the pending request and fulfills it
    const statusOnNode = await vfsNode.status('box');
    assert.strictEqual(statusOnNode, 'PENDING');

    await vfsNode.write('box', Readable.from([Buffer.from('box-data')]));

    // Browser should resolve the read
    const stream = await readPromise;
    let content = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      content += decoder.decode(chunk, { stream: true });
    }
    content += decoder.decode();
    assert.strictEqual(content, 'box-data');
    await vfsBrowser.close();
    await vfsNode.close();
  });
});
