import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';

test('VFS Link Resolution & Metadata Aggregation', async (t) => {
    const vfs = new VFS({ id: 'link-vfs', storage: new MemoryStorage() });

    // Node C: Terminal Data
    vfs.registerProvider('terminal/data', async () => {
        return new TextEncoder().encode('TERMINAL');
    }, {
        schema: { type: 'object' }
    });

    // Node B: Intermediate Link (with its own tags)
    vfs.registerProvider('link/intermediate', async (v, s) => {
        return JSON.stringify({
            geometry: 'vfs:/terminal/data',
            tags: { layer: 'blue', weight: 10 }
        });
    });

    // Node A: Root Link
    vfs.registerProvider('link/root', async (v, s) => {
        return JSON.stringify({
            geometry: 'vfs:/link/intermediate',
            tags: { owner: 'brian' }
        });
    });

    await t.test('should transparently follow multi-hop links', async () => {
        const stream = await vfs.read('link/root', {});
        assert.ok(stream !== null);
        
        const reader = stream.getReader();
        const { value } = await reader.read();
        const text = new TextDecoder().decode(value);
        assert.strictEqual(text, 'TERMINAL', 'Should return data from the end of the chain');
    });

    await t.test('should aggregate metadata along the chain', async () => {
        const cid = await vfs.getCID({ path: 'link/root', parameters: {} });
        
        // Wait for the previous test's write to finish if necessary
        await vfs.read('link/root', {}); 

        const info = vfs.storage.results.get(cid).info;
        assert.ok(info.tags, 'Should have tags');
        assert.strictEqual(info.tags.owner, 'brian', 'Should have root tag');
        assert.strictEqual(info.tags.layer, 'blue', 'Should have intermediate tag');
    });

    await t.test('should handle circular links gracefully', async () => {
        vfs.registerProvider('loop/A', async () => JSON.stringify({ geometry: 'vfs:/loop/B' }));
        vfs.registerProvider('loop/B', async () => JSON.stringify({ geometry: 'vfs:/loop/A' }));

        try {
            await vfs.read('loop/A', {});
            assert.fail('Should have thrown depth error');
        } catch (err) {
            assert.ok(err.message.includes('recursion depth exceeded'), `Expected depth error, got: ${err.message}`);
        }
    });

    await t.test('should return non-link JSON as terminal data', async () => {
        vfs.registerProvider('data/json', async () => JSON.stringify({ just: 'data' }));
        const stream = await vfs.read('data/json', {});
        const reader = stream.getReader();
        const { value } = await reader.read();
        const json = JSON.parse(new TextDecoder().decode(value));
        assert.strictEqual(json.just, 'data');
    });
});
