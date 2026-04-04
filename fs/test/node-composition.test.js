import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';
import { Readable } from 'stream';

test('Node with Parameter Composition', async (t) => {
  const vfs = new VFS();

  const node = new Node(vfs, {
    sockets: {
      input: In('test/in', { base: 1 }),
      output: Out('test/out', { base: 100 }),
    },
    async execute({ input, output }) {
      // Composition test for read()
      // Base: { base: 1 }
      // Trigger: { base: 100, trigger: 2 } (composed from tickle)
      // Call: { call: 3 }
      // Result: { base: 100, trigger: 2, call: 3 }
      const readStream = await input.read({ call: 3 });
      let data = '';
      const decoder = new TextDecoder();
      for await (const chunk of readStream) {
        data += decoder.decode(chunk, { stream: true });
      }
      data += decoder.decode();

      // Composition test for write()
      await output.write(Readable.from([data]), { call: 300 });
    },
  });

  node.start();

  await t.test(
    'read and write sockets correctly compose parameters',
    { timeout: 2000 },
    async () => {
      // 1. Manually place data at the expected composed address
      // Node.js currently merges: {...socketBase, ...trigger, ...call}
      // input socket base: { base: 1 }
      // trigger: { base: 100, trigger: 2 }
      // call: { call: 3 }
      // Result: { base: 100, trigger: 2, call: 3 }
      const expectedReadParams = { base: 100, trigger: 2, call: 3 };
      await vfs.write(
        'test/in',
        expectedReadParams,
        Readable.from(['composed-data'])
      );

      // 2. Trigger the node with some parameters
      // Node listens on { base: 100 }, so tickle must include it to match the watch
      const triggerParams = { base: 100, trigger: 2 };
      await vfs.tickle('test/out', triggerParams);

      // 3. The node should have written to the expected composed address
      const expectedWriteParams = { base: 100, trigger: 2, call: 300 };
      const resultStream = await vfs.read('test/out', expectedWriteParams);

      let result = '';
      const decoder = new TextDecoder();
      for await (const chunk of resultStream) {
        result += decoder.decode(chunk, { stream: true });
      }
      result += decoder.decode();
      assert.strictEqual(result, 'composed-data');
    }
  );

  console.log('[Test] Stopping composition node...');
  node.stop();
  console.log('[Test] Closing composition VFS...');
  await vfs.close();
  console.log('[Test] Composition VFS closed.');
});
