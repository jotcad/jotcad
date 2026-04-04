import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Node, In, Out } from '../src/node.js';
import { Readable } from 'stream';

test('AgentNode abstraction', async (t) => {
  const vfs = new VFS();

  // Agent Node 1: Geometry Box
  // Inputs: config (from a separate blackboard location)
  // Outputs: mesh
  const boxNode = new Node(vfs, {
    sockets: {
      config: In('config/box'),
      mesh: Out('geometry/box'),
    },
    async execute({ params, config, mesh }) {
      // 1. Await the parameters socket to get the triggering ID
      const { id } = await params();

      // 2. Await the input socket stream (automatically uses id from above)
      const configStream = await config.read();
      let data = '';
      const decoder = new TextDecoder();
      for await (const chunk of configStream) {
        data += decoder.decode(chunk, { stream: true });
      }
      data += decoder.decode();

      const { size } = JSON.parse(data);

      // 3. Write to the output socket
      await mesh.write(Readable.from([`Box(${size})`]));
    },
  });

  // Agent Node 2: Reparameterization Demo
  // Triggered by a request to 'process/start', but writes to a specific result path
  const processNode = new Node(vfs, {
    sockets: {
      out: Out('process/result'),
    },
    async execute({ out }) {
      // We can reparameterize the write independently of the triggering request
      await out.write(Readable.from(['Completed']), {
        status: 'success',
        timestamp: 123,
      });
    },
  });

  boxNode.start();
  processNode.start();

  await t.test(
    'Node resolves inputs from other blackboard locations',
    { timeout: 2000 },
    async () => {
      // 1. Manually place the configuration data
      await vfs.write(
        'config/box',
        { id: 'b1' },
        Readable.from([JSON.stringify({ size: 50 })])
      );

      // 2. Request the box mesh. This triggers boxNode.
      const stream = await vfs.read('geometry/box', { id: 'b1' });

      let result = '';
      const decoder = new TextDecoder();
      for await (const chunk of stream) {
        result += decoder.decode(chunk, { stream: true });
      }
      result += decoder.decode();
      assert.strictEqual(result, 'Box(50)');
    }
  );

  await t.test('Node can reparameterize output writes', { timeout: 2000 }, async () => {
    // 1. Trigger the process
    await vfs.tickle('process/result', { id: 'p1' });

    // 2. Read the reparameterized result
    const stream = await vfs.read('process/result', {
      status: 'success',
      timestamp: 123,
    });

    let result = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      result += decoder.decode(chunk, { stream: true });
    }
    result += decoder.decode();
    assert.strictEqual(result, 'Completed');
  });

  boxNode.stop();
  processNode.stop();
  await vfs.close();
});
