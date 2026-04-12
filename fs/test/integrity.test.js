import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/vfs_core.js';

test('VFS Aborted Stream Detection', async (t) => {
    await t.test('should reject stream that ends before expected length', async () => {
        const vfs = new VFS({ id: 'integrity-vfs-1', storage: new MemoryStorage() });
        // Mock a mesh link that lies about its size
        const mockMesh = {
            read: async () => {
                const body = new ReadableStream({
                    start(controller) {
                        controller.enqueue(new Uint8Array([1, 2, 3]));
                        controller.close();
                    }
                });
                return {
                    body,
                    headers: new Map([['Content-Length', '100']])
                };
            },
            stop: () => {}
        };
        vfs.mesh = mockMesh;

        const result = await vfs.read('test/corrupted', {});
        assert.strictEqual(result, null, 'Corrupted stream should return null');
        
        // Ensure nothing was cached
        const cid = await vfs.getCID({ path: 'test/corrupted', parameters: {} });
        assert.strictEqual(await vfs.storage.has(cid), false, 'Partial data should not be cached');
    });

    await t.test('should allow stream that matches expected length', async () => {
        const vfs = new VFS({ id: 'integrity-vfs-2', storage: new MemoryStorage() });
        const mockMesh = {
            read: async () => {
                const data = new Uint8Array([1, 2, 3, 4, 5]);
                const body = new ReadableStream({
                    start(controller) {
                        controller.enqueue(data);
                        controller.close();
                    }
                });
                return {
                    body,
                    headers: new Map([['Content-Length', '5']])
                };
            },
            stop: () => {}
        };
        vfs.mesh = mockMesh;

        // MUST await the stream result to ensure the workPromise finishes
        let capturedCID = null;
        vfs.events.on('state', (ev) => {
            if (ev.state === 'AVAILABLE') capturedCID = ev.cid;
        });

        const stream = await vfs.read('test/valid', {});
        assert.ok(stream !== null, 'Valid stream should be allowed');
        
        assert.ok(capturedCID !== null, 'Should have captured a CID via state event');
        const exists = await vfs.storage.has(capturedCID);
        assert.ok(exists, 'Valid data should be cached');
    });
});
