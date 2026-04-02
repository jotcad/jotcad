import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, VFSClosedError } from '../src/vfs.js';
import { Node, Out } from '../src/node.js';
import { Readable } from 'stream';

test('VFS Suicide Shutdown', async (t) => {
  const vfs = new VFS();

  await t.test('VFS.close() breaks watch loops', async () => {
    let watchBroken = false;
    const watcher = vfs.watch('test');
    
    const watchPromise = (async () => {
      try {
        for await (const event of watcher) {
          // should not happen after close
        }
      } finally {
        watchBroken = true;
      }
    })();

    vfs.close();
    await watchPromise;
    assert.strictEqual(watchBroken, true);
  });

  await t.test('Sockets throw VFSClosedError after shutdown', async () => {
    const vfs2 = new VFS();
    vfs2.close();
    
    try {
      await vfs2.read('test');
      assert.fail('Should have thrown');
    } catch (e) {
      assert.ok(e instanceof VFSClosedError);
    }
  });

  await t.test('Nodes terminate automatically when VFS is closed', async () => {
    const vfs3 = new VFS();
    let executionStarted = false;
    let executionEnded = false;

    const node = new Node(vfs3, {
      sockets: { out: Out('trigger') },
      async execute({ out }) {
        executionStarted = true;
        try {
          // Block on a read that will never be fulfilled
          await vfs3.read('never');
        } finally {
          executionEnded = true;
        }
      }
    });

    const nodePromise = node.start();
    await vfs3.tickle('trigger');
    
    // Wait for agent to start executing
    while (!executionStarted) await new Promise(r => setTimeout(r, 10));

    // Kill the VFS while the agent is blocked on the read
    vfs3.close();

    await nodePromise;
    assert.strictEqual(executionEnded, true);
  });
});
