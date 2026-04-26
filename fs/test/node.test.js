import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, Selector } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';

test('Node Core Functionality', async (t) => {
  const vfs = new VFS();
  await vfs.init();

  const boxNode = new Node({
    vfs,
    sockets: {
      config: In('config/box'),
      mesh: Out('geometry/box'),
    },
    async execute({ config, mesh }) {
      const data = await config.readData();
      if (!data) return;
      await mesh.writeData(`Box(${data.size})`);
    },
  });

  const processNode = new Node({
    vfs,
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

  await t.test('Node resolves inputs from other blackboard locations', async () => {
    await vfs.writeData(new Selector('config/box', { id: 'test' }), { size: 10 });
    const result = await vfs.readText(new Selector('process/status', { id: 'test' }));
    assert.strictEqual(result, 'Completed');
  });

  await t.test('Node can reparameterize output writes', async () => {
    await vfs.writeData(new Selector('config/box', { id: 'other' }), { size: 5 });
    const result = await vfs.readText(new Selector('process/status', { id: 'other' }));
    assert.strictEqual(result, 'Completed');
  });

  await vfs.close();
});
