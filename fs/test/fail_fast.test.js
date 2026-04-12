import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';

test('VFS Fail-Fast Validation', async (t) => {
    const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });

    vfs.registerProvider('test/op', async (v, s) => {
        return new TextEncoder().encode('success');
    }, {
        schema: {
            type: 'object',
            required: ['radius'],
            properties: {
                radius: { type: 'number' },
                variant: { type: 'string', enum: ['a', 'b'] }
            }
        }
    });

    await t.test('should reject request with missing required parameter', async () => {
        try {
            await vfs.read('test/op', {});
            assert.fail('Should have thrown an error');
        } catch (err) {
            assert.ok(err.message.includes('Missing required parameter'));
        }
    });

    await t.test('should reject request with invalid parameter type', async () => {
        try {
            await vfs.read('test/op', { radius: 'not-a-number' });
            assert.fail('Should have thrown an error');
        } catch (err) {
            assert.ok(err.message.includes('Expected number'));
        }
    });

    await t.test('should reject request with invalid enum value', async () => {
        try {
            await vfs.read('test/op', { radius: 10, variant: 'invalid' });
            assert.fail('Should have thrown an error');
        } catch (err) {
            assert.ok(err.message.includes('Expected one of [a, b]'));
        }
    });

    await t.test('should allow valid request', async () => {
        const stream = await vfs.read('test/op', { radius: 10, variant: 'a' });
        assert.ok(stream !== null);
        const reader = stream.getReader();
        const { value } = await reader.read();
        assert.strictEqual(new TextDecoder().decode(value), 'success');
    });

    await t.test('should support learned schemas via addSchema', async () => {
        vfs.addSchema('remote/op', {
            type: 'object',
            required: ['id']
        });

        try {
            await vfs.read('remote/op', {});
            assert.fail('Should have thrown an error');
        } catch (err) {
            assert.ok(err.message.includes('Missing required parameter \'id\''));
        }
    });
});
