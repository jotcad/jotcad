import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs.js';
import { Node } from '../src/node.js';
import { Readable } from 'stream';

test('Unified Permissioned Sockets Node', async (t) => {
  const vfs = new VFS();

  // Agent Node 1: Geometry Box using unified sockets
  const boxNode = new Node(vfs, {
    sockets: {
      mesh: { path: 'geometry/box', permission: 'w' },
      config: { path: 'config/box', permission: 'r' },
    },
    trigger: 'mesh',
    async execute({ mesh, config }) {
      // 1. Read from the permissioned socket
      const configStream = await config.read();
      let data = '';
      for await (const chunk of configStream) data += chunk;

      const { size } = JSON.parse(data);

      // 2. Write to the permissioned socket
      await mesh.write(Readable.from([`Box(${size})`]));
    },
  });

  boxNode.start();

  await t.test('Node resolves from permissioned sockets', async () => {
    // 1. Manually place the configuration data
    await vfs.write(
      'config/box',
      { id: 'b1' },
      Readable.from([JSON.stringify({ size: 75 })])
    );

    // 2. Request the box mesh. This triggers the 'mesh' socket.
    const stream = await vfs.read('geometry/box', { id: 'b1' });

    let result = '';
    for await (const chunk of stream) result += chunk;
    assert.strictEqual(result, 'Box(75)');
  });

  await t.test('Node enforces permissions', async () => {
    const vfs = new VFS();
    const node = new Node(vfs, {
      sockets: {
        ro: { path: 'test', permission: 'r' },
        wo: { path: 'test', permission: 'w' },
      },
      trigger: 'wo',
      async execute({ ro, wo }) {
        assert.ok(ro.read);
        assert.strictEqual(ro.write, undefined);

        assert.ok(wo.write);
        assert.strictEqual(wo.read, undefined);

        await wo.write(Readable.from(['done']));
      },
    });
    node.start();
    await vfs.read('test');
  });
});
