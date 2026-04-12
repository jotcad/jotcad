import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, WebReadableStream } from '@jotcad/fs';
import { registerJotProvider } from '../src/provider.js';

test('JotCAD VFS Provider: Integration', async (t) => {
    const vfs = new VFS({ storage: new MemoryStorage() });
    registerJotProvider(vfs);

    // Register a mock shape provider
    vfs.registerProvider('shape/box', async (v, selector) => {
        const { width = 0 } = selector.parameters;
        const bytes = new TextEncoder().encode(`Box of size ${width}`);
        return new WebReadableStream({
            start(c) { c.enqueue(bytes); c.close(); }
        });
    });

    await t.test('evaluates a .jot file with parameters', async () => {
        // 1. "Upload" a jot file to the mesh
        await vfs.writeData('parts/bracket.jot', {}, 'box(length)');

        // 2. Request the jot file with a parameter
        const result = await vfs.readText('parts/bracket.jot', { length: 42 });
        
        assert.strictEqual(result, 'Box of size 42');
    });

    await t.test('evaluates nested .jot expressions', async () => {
        const expression = 'box({ width: L }).rx(0.1)';
        
        // Mock rotation op
        vfs.registerProvider('op/rotateX', async (v, selector) => {
            const { source, turns } = selector.parameters;
            const sourceText = await v.readText(source.path, source.parameters);
            const bytes = new TextEncoder().encode(`${sourceText} rotated ${turns} turns`);
            return new WebReadableStream({
                start(c) { c.enqueue(bytes); c.close(); }
            });
        });

        const result = await vfs.readText('jot/eval', { expression, params: { L: 100 } });
        assert.strictEqual(result, 'Box of size 100 rotated 0.1 turns');
    });
});
