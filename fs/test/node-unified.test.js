import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';

test('Unified Permissioned Sockets Node', async (t) => {
  const vfs = new VFS();
  await vfs.init();

  const boxNode = new Node(vfs, {
    sockets: {
      params: {},
      config: In('config/box'),
      mesh: Out('geometry/box'),
    },
    async execute({ _params, config, mesh }) {
      const { id } = await _params();
      const { size } = await config.readData();
      await mesh.writeData(`Box(${size})`);
    },
  });

  await boxNode.start();

  await t.test('Node resolves from permissioned sockets', async () => {
    await vfs.writeData('config/box', { id: 'b1' }, { size: 10 });
    const result = await vfs.readText('geometry/box', { id: 'b1' });
    assert.strictEqual(result, 'Box(10)');
  });

  await t.test('Node enforces permissions', async () => {
    await vfs.writeData('config/box', { id: 'b2' }, { size: 5 });
    const result = await vfs.readText('geometry/box', { id: 'b2' });
    assert.strictEqual(result, 'Box(5)');
  });

  boxNode.stop();
  await vfs.close();
});
