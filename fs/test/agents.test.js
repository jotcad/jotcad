import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Readable } from 'stream';

test('Agent-style Processors', async (t) => {
  const vfs = new VFS();
  let closed = false;

  // Agent 1: Box Generator
  const startBoxProcessor = async () => {
    try {
      for await (const req of vfs.watch('geometry/box', {
        states: ['PENDING'],
      })) {
        if (closed) break;
        const hasLease = await vfs.lease(req.path, req.parameters, 1000);
        if (!hasLease) continue;

        const { width, height, depth } = req.parameters;
        const volume = width * height * depth;
        const mesh = `Mesh(box, volume: ${volume})`;

        await vfs.write(req.path, req.parameters, Readable.from([mesh]));
      }
    } catch (e) {
      if (!closed) console.error(e);
    }
  };

  // Agent 2: Volume Calculator (Depends on Box)
  const startVolumeProcessor = async () => {
    try {
      for await (const req of vfs.watch('analytics/volume', {
        states: ['PENDING'],
      })) {
        if (closed) break;
        const hasLease = await vfs.lease(req.path, req.parameters, 1000);
        if (!hasLease) continue;

        const { sourcePath, sourceParams } = req.parameters;

        // Cascading dependency: Read the mesh from the blackboard
        const meshStream = await vfs.read(sourcePath, sourceParams);

        let meshData = '';
        const decoder = new TextDecoder();
        for await (const chunk of meshStream) {
            meshData += decoder.decode(chunk, { stream: true });
        }
        meshData += decoder.decode();

        const match = meshData.match(/volume: (\d+)/);
        const volume = match ? match[1] : '0';
        await vfs.write(req.path, req.parameters, Readable.from([volume]));
      }
    } catch (e) {
      if (!closed) console.error(e);
    }
  };

  // Start agents in background
  startBoxProcessor();
  startVolumeProcessor();

  await t.test('cascading demand works', async () => {
    // We only request the final volume.
    // This should implicitly trigger the box generator.
    const volumeStream = await vfs.read('analytics/volume', {
      sourcePath: 'geometry/box',
      sourceParams: { width: 10, height: 2, depth: 5 },
    });

    let volumeResult = '';
    const decoder = new TextDecoder();
    for await (const chunk of volumeStream) {
        volumeResult += decoder.decode(chunk, { stream: true });
    }
    volumeResult += decoder.decode();

    assert.strictEqual(volumeResult, '100'); // 10 * 2 * 5
  });

  await t.test('caching prevents redundant computation', async () => {
    let boxComputeCount = 0;

    // Modify the box processor to track calls
    const originalWrite = vfs.write;
    vfs.write = async (pathOrSelector, parameters, stream) => {
      let s;
      if (typeof pathOrSelector === 'string') {
        s = { path: pathOrSelector };
      } else {
        s = pathOrSelector;
      }
      if (s.path === 'geometry/box') {
        boxComputeCount++;
      }
      return originalWrite.call(vfs, pathOrSelector, parameters, stream);
    };

    // Request the same volume again
    await vfs.read('analytics/volume', {
      sourcePath: 'geometry/box',
      sourceParams: { width: 10, height: 2, depth: 5 },
    });

    // Box shouldn't have been re-computed
    assert.strictEqual(boxComputeCount, 0);
    
    vfs.write = originalWrite;
  });

  closed = true;
  await vfs.close();
});
