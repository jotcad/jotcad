import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';
import { Readable } from 'stream';

test('Unified Permissioned Sockets Node', async (t) => {
  const vfs = new VFS();

  // Agent Node 1: Geometry Box using unified sockets
  const boxNode = new Node(vfs, {
    sockets: {
      mesh: Out('geometry/box'),
      config: In('config/box'),
    },
    async execute({ mesh, config }) {
      // 1. Read from the permissioned socket
      const configStream = await config.read();
      let data = '';
      const decoder = new TextDecoder();
      for await (const chunk of configStream) {
          data += decoder.decode(chunk, { stream: true });
      }
      data += decoder.decode();

      const { size } = JSON.parse(data);

      // 2. Write to the permissioned socket
      await mesh.write(Readable.from([`Box(${size})`]));
    },
  });

  boxNode.start();

  await t.test('Node resolves from permissioned sockets', { timeout: 2000 }, async () => {
    // 1. Manually place the configuration data
    await vfs.write(
      'config/box',
      { id: 'b1' },
      Readable.from([JSON.stringify({ size: 75 })])
    );

    // 2. Request the box mesh. This triggers the 'mesh' socket.
    const stream = await vfs.read('geometry/box', { id: 'b1' });

    let result = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
        result += decoder.decode(chunk, { stream: true });
    }
    result += decoder.decode();
    assert.strictEqual(result, 'Box(75)');
  });

  await t.test('Node enforces permissions', { timeout: 2000 }, async () => {
    const localVfs = new VFS();
    const node = new Node(localVfs, {
      sockets: {
        ro: In('test'),
        wo: Out('test'),
      },
      async execute({ ro, wo }) {
        assert.ok(ro.read);
        assert.strictEqual(ro.write, undefined);

        assert.ok(wo.write);
        assert.strictEqual(wo.read, undefined);

        await wo.write(Readable.from(['done']));
      },
    });
    node.start();
    const stream = await localVfs.read('test');
    let content = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
        content += decoder.decode(chunk, { stream: true });
    }
    content += decoder.decode();
    assert.strictEqual(content, 'done');

    node.stop();
    await localVfs.close();
  });

  boxNode.stop();
  await vfs.close();
});
