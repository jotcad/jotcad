import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';
import { MeshLink } from '../src/mesh_link.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import http from 'node:http';

test('Discovery Protocol (Spy)', async (t) => {
  let serverA, serverB;
  let vfsA, vfsB;
  let meshA, meshB;

  t.after(async () => {
    if (serverA) serverA.close();
    if (serverB) serverB.close();
    if (meshA) meshA.stop();
    if (meshB) meshB.stop();
    if (vfsA) await vfsA.close();
    if (vfsB) await vfsB.close();
  });

  await t.test('setup network', async () => {
    vfsA = new VFS({ id: 'node-a', storage: new MemoryStorage() });
    vfsB = new VFS({ id: 'node-b', storage: new MemoryStorage() });

    serverA = http.createServer();
    serverB = http.createServer();

    meshA = new MeshLink(vfsA, ['http://localhost:19902'], {
      localUrl: 'http://localhost:19901',
    });
    meshB = new MeshLink(vfsB, ['http://localhost:19901'], {
      localUrl: 'http://localhost:19902',
    });

    registerVFSRoutes(vfsA, serverA, '', meshA);
    registerVFSRoutes(vfsB, serverB, '', meshB);

    await new Promise((r) => serverA.listen(19901, '127.0.0.1', r));
    await new Promise((r) => serverB.listen(19902, '127.0.0.1', r));

    await meshA.start();
    await meshB.start();
  });

  await t.test('spy returns multiplexed VFS bundle', async () => {
    // Proactively provision data on both nodes
    await vfsA.writeData('shape/box', { w: 10 }, { from: 'A' });

    await vfsB.writeData('shape/box', { w: 20 }, { from: 'B' });
    await vfsB.writeData('shape/sphere', { r: 5 }, { from: 'B' });

    // Query Node A. It should gather from A and B using passive storage scanning.
    const stream = await vfsA.spy('shape/*', {});
    assert.ok(stream, 'Spy should return a stream');

    // Read the stream and verify the chunks
    const reader = stream.getReader();
    const chunks = [];
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }
    } finally {
      reader.releaseLock();
    }

    const combined = Buffer.concat(
      chunks.map((c) => Buffer.from(c))
    ).toString();

    console.log('COMBINED:', combined);

    // Ensure both Node A and Node B results are present
    assert.ok(combined.includes('"shape/box"'), 'Should contain box selector');
    assert.ok(
      combined.includes('"shape/sphere"'),
      'Should contain sphere selector'
    );
    assert.ok(combined.includes('"from": "A"'), 'Should contain data from A');
    assert.ok(combined.includes('"from": "B"'), 'Should contain data from B');

    // Verify VFS bundle structure (\n=LEN NAME\nCONTENT)
    assert.ok(combined.startsWith('\n='), 'Should start with VFS header');
  });
});
