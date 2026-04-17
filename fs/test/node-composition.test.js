import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';

test('Node with Parameter Composition', async (t) => {
  const vfs = new VFS();
  await vfs.init();

  const compositionNode = new Node(vfs, {
    sockets: {
      params: {},
      input: In('test/in', { base: 100 }),
      output: Out('test/out'),
    },
    async execute({ _params, input, output }) {
      const p = await _params();
      const val = await input.readData();
      await output.writeData(`composed-${val}`);
    },
  });

  await compositionNode.start();

  await t.test(
    'read and write sockets correctly compose parameters',
    async () => {
      // Write data with a 'trigger' parameter
      await vfs.writeData('test/in', { base: 100, trigger: 2 }, 'data');

      // Read with the same 'trigger' parameter
      const result = await vfs.readText('test/out', { trigger: 2 });
      assert.strictEqual(result, 'composed-data');
    }
  );

  compositionNode.stop();
  await vfs.close();
});
