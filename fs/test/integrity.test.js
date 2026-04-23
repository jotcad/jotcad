import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, WebReadableStream } from '../src/vfs_core.js';

test('VFS Aborted Stream Detection', async (t) => {
  const vfs = new VFS({ id: 'integrity-vfs', storage: new MemoryStorage() });

  await t.test('should reject stream that ends before expected length', async () => {
    const selector = { path: 'corrupted/data', parameters: {} };
    const bytes = new Uint8Array([1, 2, 3]);
    
    // We simulate a stream that claims 100 bytes but only sends 3
    const stream = new WebReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      }
    });

    try {
      await vfs.write(selector, stream, { size: 100 });
      assert.fail('Should have thrown size mismatch error');
    } catch (err) {
      assert.ok(err.message.includes('Expected 100'));
    }

    const result = await vfs.read(selector);
    assert.strictEqual(result, null, 'Corrupted stream should return null');
  });

  await t.test('should allow stream that matches expected length', async () => {
    const selector = { path: 'valid/data', parameters: {} };
    const bytes = new Uint8Array([1, 2, 3, 4, 5]);
    
    const stream = new WebReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      }
    });

    await vfs.write(selector, stream, { size: 5 });
    const result = await vfs.read(selector);
    assert.ok(result !== null, 'Valid stream should be allowed');
  });

  await vfs.close();
});
