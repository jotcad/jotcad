import test from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs.js';
import { Selector } from '../src/cid.js';

test('VFS Deadlock Regression: Provider returning Selector', { timeout: 5000 }, async (t) => {
    const vfs = new VFS('test-node');
    
    // 1. A provider that simply returns data
    vfs.registerProvider('target/data', async (v, s) => {
        return { 
            stream: new ReadableStream({
                start(c) {
                    c.enqueue(new TextEncoder().encode("FINAL_DATA"));
                    c.close();
                }
            }),
            metadata: { state: 'AVAILABLE' }
        };
    });

    // 2. A provider that returns a Selector (Alias)
    // In the broken implementation, this causes a deadlock because it attempts 
    // to re-read the same source key which is already locked in activeWait.
    vfs.registerProvider('source/alias', async (v, s) => {
        console.log('[Test] source/alias provider triggered');
        return new Selector('target/data');
    });

    console.log('[Test] Starting read of source/alias...');
    try {
        const result = await vfs.read(new Selector('source/alias'));
        console.log('[Test] Read complete');
        
        const reader = result.stream.getReader();
        const { value } = await reader.read();
        const text = new TextDecoder().decode(value);
        
        assert.strictEqual(text, 'FINAL_DATA');
    } catch (err) {
        console.error('[Test] Read failed:', err);
        throw err;
    }
});
