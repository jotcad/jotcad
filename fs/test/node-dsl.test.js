import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';

test('Node with In/Out DSL', async (t) => {
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

  const processNode = new Node(vfs, {
    sockets: {
      mesh: In('geometry/box'),
      status: Out('process/status'),
    },
    async execute({ mesh, status }) {
      const data = await mesh.readData();
      if (data) await status.writeData('Completed');
    },
  });

  await boxNode.start();
  await processNode.start();

  await t.test('cascading dependency using In/Out DSL', async () => {
    await vfs.writeData('config/box', { id: 'b1' }, { size: 10 });
    const result = await vfs.readData('process/status', { id: 'b1' });
    assert.strictEqual(result, 'Completed');
  });

  await t.test('Node reparameterization with Out socket', async () => {
    await vfs.writeData('config/box', { id: 'b2' }, { size: 20 });
    const result = await vfs.readData('process/status', { id: 'b2' });
    assert.strictEqual(result, 'Completed');
  });

  boxNode.stop();
  processNode.stop();
  await vfs.close();
});
