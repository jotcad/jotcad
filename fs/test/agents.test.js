import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { WebReadableStream } from '../src/vfs_core.js';

test('Agent-style Processors (as Providers)', async (t) => {
  const vfs = new VFS({ id: 'test-vfs' });
  await vfs.init();

  // Op 1: Box Generator
  vfs.registerProvider('geometry/box', async (v, selector) => {
    const { width, height, depth } = selector.parameters;
    const volume = width * height * depth;
    const mesh = `Mesh(box, volume: ${volume})`;
    const bytes = new TextEncoder().encode(mesh);
    return new WebReadableStream({
        start(c) { c.enqueue(bytes); c.close(); }
    });
  });

  // Op 2: Volume Calculator (Depends on Box)
  vfs.registerProvider('analytics/volume', async (v, selector, context) => {
    const { sourcePath, sourceParams } = selector.parameters;

    // Cascading dependency: Read the mesh from the same VFS
    // We pass the context (stack) to prevent loops
    const meshData = await v.readText(sourcePath, sourceParams, context);
    if (!meshData) return null;

    const match = meshData.match(/volume: (\d+)/);
    const volume = match ? match[1] : '0';
    const bytes = new TextEncoder().encode(volume);
    
    return new WebReadableStream({
        start(c) { c.enqueue(bytes); c.close(); }
    });
  });

  await t.test('cascading demand works', async () => {
    // We only request the final volume.
    // This should implicitly trigger the box generator via the provider chain.
    const volume = await vfs.readText('analytics/volume', {
      sourcePath: 'geometry/box',
      sourceParams: { width: 10, height: 2, depth: 5 },
    });

    assert.strictEqual(volume, '100'); // 10 * 2 * 5
  });

  await t.test('caching prevents redundant computation', async () => {
    let boxComputeCount = 0;

    // Wrap the box provider to track calls
    const originalProvider = vfs.providers.get('geometry/box');
    vfs.registerProvider('geometry/box', async (v, s, c) => {
        boxComputeCount++;
        return originalProvider(v, s, c);
    });

    // Request the same volume again
    await vfs.readData('analytics/volume', {
      sourcePath: 'geometry/box',
      sourceParams: { width: 10, height: 2, depth: 5 },
    });

    // Box shouldn't have been re-computed because it's in local storage
    assert.strictEqual(boxComputeCount, 0);
  });

  await vfs.close();
});
