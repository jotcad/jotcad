import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';
import { MeshLink } from '../src/mesh_link.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import http from 'node:http';

async function consumeJSON(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return JSON.parse(new TextDecoder().decode(bytes));
}

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

    await vfsA.init();
    await vfsB.init();

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

  await t.test('spy returns multiplexed VFS metadata bundle', async () => {
    // Proactively provision data on both nodes with discoverable tags
    await vfsA.write(new Selector('shape/box', { w: 10 }), { tags: { from: 'A' } }, { encoding: 'json' });

    await vfsB.write(new Selector('shape/box', { w: 20 }), { tags: { from: 'B' } }, { encoding: 'json' });
    await vfsB.write(new Selector('shape/sphere', { r: 5 }), { tags: { from: 'B' } }, { encoding: 'json' });

    // Wait for mesh propagation
    await new Promise(r => setTimeout(r, 500));

    // Query Node A. It should gather from A and B using passive storage scanning.
    const stream = await vfsA.spy(new Selector('shape/*'), {});
    assert.ok(stream, 'Spy should return a stream');

    const reader = stream.getReader();
    const results = [];
    let buffer = '';
    
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        buffer += new TextDecoder().decode(value);
        
        // Very basic VFS bundle parser
        while (buffer.includes('\n=')) {
            const start = buffer.indexOf('\n=');
            const headerEnd = buffer.indexOf('\n', start + 2);
            if (headerEnd === -1) break;
            
            const header = buffer.slice(start + 2, headerEnd);
            const [lenStr, cid] = header.split(' ');
            const len = parseInt(lenStr);
            
            if (buffer.length < headerEnd + 1 + len) break;
            
            const content = buffer.slice(headerEnd + 1, headerEnd + 1 + len);
            results.push({ cid, metadata: JSON.parse(content) });
            buffer = buffer.slice(headerEnd + 1 + len);
        }
    }

    console.log('RESULTS FOUND:', results.length);
    for (const r of results) {
        console.log(` - ${r.cid}: ${r.metadata.selector.path}`);
    }

    // Ensure both Node A and Node B results are present in metadata
    const paths = results.map(r => r.metadata.selector.path);
    assert.ok(paths.includes('shape/box'), 'Should contain box selector');
    assert.ok(paths.includes('shape/sphere'), 'Should contain sphere selector');

    // To verify the tags, we must READ the data (Pure Router Model)
    let foundA = false;
    let foundB = false;

    for (const res of results) {
        const { stream: dataStream } = await vfsA.read(res.cid);
        const data = await consumeJSON(dataStream);
        if (data.tags?.from === 'A') foundA = true;
        if (data.tags?.from === 'B') foundB = true;
    }

    assert.ok(foundA, 'Should have found data from Node A via subsequent read');
    assert.ok(foundB, 'Should have found data from Node B via subsequent read');
  });
});
