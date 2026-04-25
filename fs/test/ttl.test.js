import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';

test('VFS TTL Enforcement', async (t) => {
  const vfs = new VFS({ id: 'ttl-vfs', storage: new MemoryStorage() });
  await vfs.init();

  vfs.registerProvider('test/op', async () => {
    return new TextEncoder().encode('fresh');
  });

  await t.test('should reject request that is already expired', async () => {
    const past = Date.now() - 1000;
    const result = await vfs.read(new Selector('test/op'), { expiresAt: past });
    assert.strictEqual(result, null, 'Expired request should return null');
  });

  await t.test('should allow request with future expiration', async () => {
    const future = Date.now() + 5000;
    const stream = await vfs.read(new Selector('test/op'), { expiresAt: future });
    assert.ok(stream !== null, 'Fresh request should be allowed');

    const reader = stream.getReader();
    const { value } = await reader.read();
    assert.strictEqual(new TextDecoder().decode(value), 'fresh');
  });
});
