import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs.js';
import { Node, In, Out } from '../src/node.js';
import { Readable } from 'stream';

test('Node with Parameter Composition', async (t) => {
  const vfs = new VFS();

  const node = new Node(vfs, {
    sockets: {
      input: In('test/in', { base: 1 }),
      output: Out('test/out', { base: 100 })
    },
    async execute({ input, output }) {
      // Composition test for read()
      // Base: { base: 1 }
      // Trigger: { trigger: 2 }
      // Call: { call: 3 }
      // Result: { base: 1, trigger: 2, call: 3 }
      const readStream = await input.read({ call: 3 });
      let data = '';
      for await (const chunk of readStream) data += chunk;
      
      // Composition test for write()
      await output.write(Readable.from([data]), { call: 300 });
    }
  });

  node.start();

  await t.test('read and write sockets correctly compose parameters', async () => {
    // 1. Manually place data at the expected composed address
    const expectedReadParams = { base: 1, trigger: 2, call: 3 };
    await vfs.write('test/in', expectedReadParams, Readable.from(['composed-data']));

    // 2. Trigger the node with some parameters
    const triggerParams = { trigger: 2 };
    await vfs.tickle('test/out', triggerParams);

    // 3. The node should have written to the expected composed address
    const expectedWriteParams = { base: 100, trigger: 2, call: 300 };
    const resultStream = await vfs.read('test/out', expectedWriteParams);
    
    let result = '';
    for await (const chunk of resultStream) result += chunk;
    assert.strictEqual(result, 'composed-data');
  });
});
