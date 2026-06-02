import test from 'node:test';
import assert from 'node:assert';
import { launchSystem } from '../orchestrator.js';
import { VFS, MemoryStorage, MeshLink, Selector } from '../fs/src/index.js';

test('Sovereign Packet Protocol Integration', async (t) => {
    // Launch the orchestrator profile which spins up a C++ ops node and a JS export node
    // Enable log capturing to trace the Sovereign Packet headers
    const system = await launchSystem('test/sovereign_js', 'DEBUG', { captureLogs: true });

    // Create a local client VFS node to interact with the mesh
    const clientVfs = new VFS({ id: 'test_client', storage: new MemoryStorage() });
    await clientVfs.init();
    
    // Wait for Node A to be up and running
    let nodeAReady = false;
    for (let i = 0; i < 50; i++) {
        try {
            const resp = await globalThis.fetch('http://localhost:8181/health');
            if (resp.ok) {
                nodeAReady = true;
                break;
            }
        } catch (e) {}
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    if (!nodeAReady) {
        throw new Error('Node A failed to start in time');
    }

    // Connect to node A
    const meshLink = new MeshLink(clientVfs, ['http://localhost:8181']);
    await meshLink.start();

    t.after(async () => {
        meshLink.stop();
        await system.stop();
    });

    // Wait for mesh topology to settle
    await new Promise(resolve => setTimeout(resolve, 2000));

    await t.test('Client can write a binary packet to the mesh and retrieve it', async () => {
        const payload = new Uint8Array([10, 20, 30, 40, 50]);
        const selector = Selector.fromObject({ path: 'test/integration_binary', parameters: {} });
        
        // Write the binary packet to the local VFS storage
        await clientVfs.write(selector, payload, { encoding: 'bytes' });

        // Query Node A to pull the data from the client via reverse long-polling
        try {
            const resp = await globalThis.fetch('http://localhost:8181/read_selector', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'x-vfs-op': 'READ_SELECTOR',
                    'x-vfs-selector': JSON.stringify(selector)
                }
            });

            assert.strictEqual(resp.status, 200, 'Expected Node A to return 200 OK');
            assert.strictEqual(resp.headers.get('x-vfs-encoding'), 'bytes', 'Expected binary packet encoding');

            const buffer = await resp.arrayBuffer();
            const received = new Uint8Array(buffer);
            assert.deepStrictEqual(Array.from(received), [10, 20, 30, 40, 50]);
        } catch (e) {
            // Log assertions
            const cxxLogs = system.logs.filter(l => l.includes('Ops Node'));
            const jsLogs = system.logs.filter(l => l.includes('Export Node'));
            
            console.log(`Captured ${cxxLogs.length} C++ logs and ${jsLogs.length} JS logs.`);
            
            // Re-throw so we can see the hang/failure
            throw e;
        }
    });
});
