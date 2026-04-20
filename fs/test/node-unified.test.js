import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';

test('Node Unified Resolution', async (t) => {
  const vfs = new VFS();
  await vfs.init();

  const boxNode = new Node(vfs, {
    sockets: {
      config: In('config/box'),
      mesh: Out('geometry/box'),
    },
    async execute({ config, mesh }) {
      const data = await config.readData();
      if (!data) return;
      const { size } = data;
      await mesh.writeData(`Box(${size})`);
    },
  });

  await boxNode.start();

  await t.test('Node resolves from permissioned sockets', async () => {
    await vfs.writeData({ path: 'config/box', parameters: { id: 1 } }, { size: 10 });
    const result = await vfs.readText({ path: 'geometry/box', parameters: { id: 1 } });
    assert.strictEqual(result, 'Box(10)');
  });

  await t.test('Node enforces permissions', async () => {
    await vfs.writeData({ path: 'config/box', parameters: { id: 2 } }, { size: 5 });
    const result = await vfs.readText({ path: 'geometry/box', parameters: { id: 2 } });
    assert.strictEqual(result, 'Box(5)');
  });

  await vfs.close();
});
