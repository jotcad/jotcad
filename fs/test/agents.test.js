import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs.js';
import { Readable } from 'stream';

test('Agent-style Processors', async (t) => {
  const vfs = new VFS();

  // Agent 1: Box Generator
  const startBoxProcessor = async () => {
    for await (const req of vfs.watch('geometry/box', {
      states: ['PENDING'],
    })) {
      const hasLease = await vfs.lease(req.path, req.parameters, 1000);
      if (!hasLease) continue;

      const { width, height, depth } = req.parameters;
      const volume = width * height * depth;
      const mesh = `Mesh(box, volume: ${volume})`;

      await vfs.write(req.path, req.parameters, Readable.from([mesh]));
    }
  };

  // Agent 2: Volume Calculator (Depends on Box)
  const startVolumeProcessor = async () => {
    for await (const req of vfs.watch('analytics/volume', {
      states: ['PENDING'],
    })) {
      const hasLease = await vfs.lease(req.path, req.parameters, 1000);
      if (!hasLease) continue;

      const { sourcePath, sourceParams } = req.parameters;

      // Cascading dependency: Read the mesh from the blackboard
      const meshStream = await vfs.read(sourcePath, sourceParams);

      let meshData = '';
      for await (const chunk of meshStream) meshData += chunk;

      const volume = meshData.match(/volume: (\d+)/)[1];
      await vfs.write(req.path, req.parameters, Readable.from([volume]));
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
    for await (const chunk of volumeStream) volumeResult += chunk;

    assert.strictEqual(volumeResult, '100'); // 10 * 2 * 5
  });

  await t.test('caching prevents redundant computation', async () => {
    let boxComputeCount = 0;

    // Modify the box processor to track calls
    const originalWrite = vfs.write;
    vfs.write = async (selector, stream) => {
      if (typeof selector === 'object' && selector.path === 'geometry/box') {
        boxComputeCount++;
      }
      return originalWrite.call(vfs, selector, stream);
    };

    // Request the same volume again
    await vfs.read('analytics/volume', {
      sourcePath: 'geometry/box',
      sourceParams: { width: 10, height: 2, depth: 5 },
    });

    // Box shouldn't have been re-computed
    assert.strictEqual(boxComputeCount, 0);
  });
});
