import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../src/index.js';
import { MeshLink, Selector } from '../src/index.js';

test('ReverseConnection should drain ReadableStream into Uint8Array', async (t) => {
    const vfs = new VFS({ id: 'browser', storage: new MemoryStorage() });
    await vfs.init();
    
    const mesh = new MeshLink(vfs, []);
    
    // 1. Setup a dummy result that is a ReadableStream
    const content = new TextEncoder().encode('Hello Mesh!');
    const createStream = () => new ReadableStream({
        start(controller) {
            controller.enqueue(content);
            controller.close();
        }
    });
    
    // 2. Mock fetch to capture the request body
    let capturedBodies = [];
    let pollCount = 0;
    
    const mockFetch = async (url, options) => {
        pollCount++;
        console.log(`[MockFetch] Poll ${pollCount}, hasBody: ${!!options.body}`);
        
        if (options.body) {
            capturedBodies.push(options.body);
        }

        // Simulate a "READ" command from the server on the first poll
        // Format MUST match mesh_link.js:316
        if (pollCount === 1) {
            return {
                status: 200,
                text: async () => JSON.stringify({
                    type: 'COMMAND',
                    op: 'READ',
                    id: 'server-request-123',
                    selector: { path: 'test/path', parameters: {} },
                    stack: [],
                    resolutionStack: [],
                    expiresAt: Date.now() + 30000
                })
            };
        }

        // On subsequent polls, return 204 after yielding the event loop
        await new Promise(resolve => setTimeout(resolve, 10));
        return { status: 204 };
    };

    // 3. Manually create and start a ReverseConnection
    const { ReverseConnection } = await import('../src/mesh_link.js');
    const rev = new ReverseConnection('server-poller', mesh, { instanceId: 'test-inst' });
    
    // Mock vfs.read to return our stream
    vfs.read = async () => ({ stream: createStream(), metadata: { encoding: 'bytes' } });

    console.log('[Test] Starting ReverseConnection polling loop...');
    
    // Start polling in background
    const pollPromise = rev.startPolling('http://mock-server/vfs', mockFetch);

    // Give it enough time to complete multiple iterations
    console.log('[Test] Entering wait loop...');
    for (let i = 0; i < 10; i++) {
        console.log(`[Test] Wait iteration ${i}, capturedBodies: ${capturedBodies.length}`);
        await new Promise(resolve => setTimeout(resolve, 50));
        if (capturedBodies.length > 0) {
            console.log('[Test] Found captured body, breaking wait loop.');
            break;
        }
    }
    
    console.log('[Test] Stopping ReverseConnection...');
    rev.stop(); 
    console.log('[Test] Waiting for pollPromise to resolve...');
    try { await pollPromise; } catch(e) {
        console.log('[Test] pollPromise rejected (expected on stop):', e.message);
    }
    console.log('[Test] pollPromise resolved/settled.');

    // 4. Verify results
    console.log('[Test] Captured Bodies:', capturedBodies.length);
    if (capturedBodies.length > 0) {
        const capturedBody = capturedBodies[0];
        console.log('[Test] Captured Body Type:', capturedBody?.constructor?.name);
        
        assert.ok(capturedBody instanceof Uint8Array, 'Body should be a Uint8Array, not a ReadableStream');
        assert.strictEqual(new TextDecoder().decode(capturedBody), 'Hello Mesh!');
        console.log('[Test] SUCCESS: Stream was correctly drained into a buffer.');
    } else {
        throw new Error('No body was captured in the second poll iteration. READ command might have failed to execute.');
    }
});
