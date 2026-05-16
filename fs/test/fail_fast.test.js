import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, Selector } from '../src/index.js';
import { vfsResult } from './vfs_test_helpers.js';

test('VFS Fail-Fast Validation', async (t) => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  vfs.registerProvider(
    'test/op',
    async (v, s) => {
      return vfsResult('success', { encoding: 'binary' });
    },
    {
      schema: {
        type: 'object',
        required: ['diameter'],
        properties: {
          diameter: { type: 'number' },
          variant: { type: 'string', enum: ['a', 'b'] },
        },
      },
    }
  );

  await t.test(
    'should reject request with missing required parameter',
    async () => {
      try {
        await vfs.read(new Selector('test/op', {}));
        assert.fail('Should have thrown an error');
      } catch (err) {
        assert.ok(err.message.includes('Missing required parameter'));
      }
    }
  );

  await t.test(
    'should reject request with invalid parameter type',
    async () => {
      try {
        await vfs.read(new Selector('test/op', { diameter: 'not-a-number' }));
        assert.fail('Should have thrown an error');
      } catch (err) {
        assert.ok(err.message.includes('Invalid parameter type'));
      }
    }
  );

  await t.test('should reject request with invalid enum value', async () => {
    try {
      await vfs.read(new Selector('test/op', { diameter: 20, variant: 'invalid' }));
      assert.fail('Should have thrown an error');
    } catch (err) {
      assert.ok(err.message.includes('Invalid enum value'));
    }
  });

  await t.test('should allow valid request', async () => {
    const result = await vfs.read(new Selector('test/op', { diameter: 20, variant: 'a' }));
    assert.ok(result !== null);
    const { stream } = result;
    const reader = stream.getReader();
    const { value } = await reader.read();
    assert.strictEqual(new TextDecoder().decode(value), 'success');
  });

  await t.test('should support learned schemas via addSchema', async () => {
    vfs.addSchema('remote/op', {
      type: 'object',
      required: ['id'],
    });

    try {
      await vfs.read(new Selector('remote/op', {}));
      assert.fail('Should have thrown an error');
    } catch (err) {
      assert.ok(err.message.includes("Missing required parameter 'id'"));
    }
  });

  await vfs.close();
});
