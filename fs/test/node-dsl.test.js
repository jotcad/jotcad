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
      const data = await config.readData();
      if (!data) return;
      const { size } = data;
      await mesh.writeData(`Box(${size})`);
    },
  });

  const processNode = new Node(vfs, {
    sockets: {
      mesh: In('geometry/box'),
      status: Out('process/status'),
    },
    async execute({ mesh, status }) {
      const data = await mesh.readText();
      if (data) await status.writeData('Completed');
    },
  });

  await boxNode.start();
  await processNode.start();

  await t.test('cascading dependency using In/Out DSL', async () => {
    // 1. Provision the leaf config
    await vfs.writeData({ path: 'config/box', parameters: { id: 'test' } }, { size: 10 });
    
    // 2. Trigger the chain
    const result = await vfs.readText({ path: 'process/status', parameters: { id: 'test' } });
    assert.strictEqual(result, 'Completed');
  });

  await t.test('Node reparameterization with Out socket', async () => {
    await vfs.writeData({ path: 'config/box', parameters: { id: 'other' } }, { size: 5 });
    const result = await vfs.readText({ path: 'process/status', parameters: { id: 'other' } });
    assert.strictEqual(result, 'Completed');
  });

  await vfs.close();
});
