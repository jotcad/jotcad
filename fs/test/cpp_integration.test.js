import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import http from 'http';
import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('C++ Agent Integration', { timeout: 15000 }, async (t) => {
    const vfs = new VFS({ id: 'hub' });
    const server = http.createServer();
    registerVFSRoutes(vfs, server, '/vfs');

    const port = await new Promise(resolve => server.listen(0, '127.0.0.1', () => resolve(server.address().port)));
    const baseUrl = `http://127.0.0.1:${port}/vfs`;

    // 1. Spawn C++ Agent
    const cppAgentPath = path.join(__dirname, '..', 'cpp', 'bin', 'vfs_example_agent');
    const cppAgent = spawn(cppAgentPath, [baseUrl]);

    cppAgent.stdout.on('data', (data) => console.log(`[C++ stdout] ${data}`));
    cppAgent.stderr.on('data', (data) => console.error(`[C++ stderr] ${data}`));

    await t.test('C++ agent fulfills demand on the hub', async () => {
        // Wait for C++ agent to be ready
        await new Promise(r => setTimeout(r, 1000));

        // Step 1: Express demand for a box on the Hub
        const boxPath = 'geometry/box/small';
        const boxParams = { size: 10 };
        
        console.log('[Hub] Expressing demand for box...');
        await vfs.tickle(boxPath, boxParams);

        // Step 2: Wait for C++ agent to fulfill it
        console.log('[Hub] Waiting for C++ agent to fill result...');
        const stream = await vfs.read(boxPath, boxParams);
        
        let result = '';
        const decoder = new TextDecoder();
        for await (const chunk of stream) {
            result += decoder.decode(chunk, { stream: true });
        }
        result += decoder.decode();

        console.log(`[Hub] Received result: ${result}`);
        assert.ok(result.includes('Mesh(box, size={"size":10})'));
    });

    // Cleanup
    console.log('[Test] Cleaning up C++ Agent test...');
    cppAgent.kill('SIGKILL');
    server.close();
    await vfs.close();
    console.log('[Test] C++ Agent test cleanup complete.');
});
