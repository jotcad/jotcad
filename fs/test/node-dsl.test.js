import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';
import { Readable } from 'stream';

test('Node with In/Out DSL', async (t) => {
  const vfs = new VFS();

  // Agent: Box Generator
  // Uses 'In' for configuration and 'Out' for result mesh.
  const boxNode = new Node(vfs, {
    sockets: {
      config: In('config/box'),
      mesh: Out('geometry/box'),
    },
    async execute({ config, mesh }) {
      // 1. Explicit .read()
      const configStream = await config.read();
      let data = '';
      const decoder = new TextDecoder();
      for await (const chunk of configStream) {
        data += decoder.decode(chunk, { stream: true });
      }
      data += decoder.decode();

      const { size } = JSON.parse(data);

      // 2. Explicit .write()
      await mesh.write(Readable.from([`Box(${size})`]));
    },
  });

  boxNode.start();

  await t.test('cascading dependency using In/Out DSL', { timeout: 2000 }, async () => {
    // 1. Manually place the configuration data on the blackboard
    await vfs.write(
      'config/box',
      { id: 'b1' },
      Readable.from([JSON.stringify({ size: 100 })])
    );

    // 2. Request the box mesh.
    // This triggers the Out socket 'mesh' on the boxNode.
    const stream = await vfs.read('geometry/box', { id: 'b1' });

    let result = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      result += decoder.decode(chunk, { stream: true });
    }
    result += decoder.decode();
    assert.strictEqual(result, 'Box(100)');
  });

  await t.test('Node reparameterization with Out socket', { timeout: 2000 }, async (t) => {
    const vfs = new VFS();
    const node = new Node(vfs, {
      sockets: {
        trigger: Out('process/trigger'),
        result: Out('process/result'),
      },
      async execute({ result }) {
        // Explicit .write()
        await result.write(Readable.from(['Success']), { status: 'final' });
      },
    });

    node.start();

    // Trigger the process
    await vfs.tickle('process/trigger', { id: 'job1' });

    // Read the reparameterized result
    const stream = await vfs.read('process/result', { status: 'final' });
    let content = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
        content += decoder.decode(chunk, { stream: true });
    }
    content += decoder.decode();
    assert.strictEqual(content, 'Success');

    console.log('[Test] Stopping reparam node...');
    node.stop();
    await vfs.close();
  });

  console.log('[Test] Stopping box node and closing VFS...');
  boxNode.stop();
  await vfs.close();
  console.log('[Test] Node DSL cleanup complete.');
});
