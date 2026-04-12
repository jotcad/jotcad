import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../../fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';

const PORT_OPS = 19801;
const PORT_EXPORT = 19802;
const PORT_CLIENT = 19803;

test('Full Mesh Pipeline (C++ Ops + JS Export)', async (t) => {
    let opsProcess, exportProcess, clientServer, clientVfs, mesh;
    const filename = 'pipeline_test_result.pdf';

    t.after(async () => {
        console.log('[Test Pipeline] Cleaning up...');
        if (mesh) mesh.stop();
        if (clientVfs) await clientVfs.close();
        if (clientServer) {
            clientServer.close();
            // Force close connections after a short delay
            if (typeof clientServer.closeAllConnections === 'function') {
                clientServer.closeAllConnections();
            }
        }
        
        if (opsProcess) opsProcess.kill('SIGKILL');
        if (exportProcess) exportProcess.kill('SIGKILL');
        
        await fs.rm(path.resolve('.vfs_storage_pipeline-ops'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve('.vfs_storage_pipeline-export'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve('.vfs_storage_pipeline-client'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve(filename), { force: true }).catch(() => {});
        console.log('[Test Pipeline] Cleanup complete.');
    });

    // 1. Start C++ Ops Node (Leaf)
    console.log('[Test Pipeline] Launching C++ Ops Node...');
    const opsBin = path.resolve('bin/ops');
    opsProcess = spawn(opsBin, [], {
        env: { ...process.env, PORT: PORT_OPS.toString(), PEER_ID: 'pipeline-ops' }
    });
    opsProcess.stdout.on('data', d => console.log(`[OPS] ${d}`));
    opsProcess.stderr.on('data', d => console.error(`[OPS ERR] ${d}`));

    // 2. Start JS Export Node (Peered with Ops)
    console.log('[Test Pipeline] Launching JS Export Node...');
    exportProcess = spawn('node', [path.resolve('src/export_service.js')], {
        env: { 
            ...process.env, 
            PORT: PORT_EXPORT.toString(), 
            PEER_ID: 'pipeline-export',
            NEIGHBORS: `http://localhost:${PORT_OPS}`
        }
    });
    exportProcess.stdout.on('data', d => console.log(`[EXPORT] ${d}`));
    exportProcess.stderr.on('data', d => console.error(`[EXPORT ERR] ${d}`));

    // Wait for nodes to be healthy
    const waitForHealth = async (url, name) => {
        console.log(`[Test Pipeline] Waiting for ${name} health...`);
        for (let i = 0; i < 20; i++) {
            try {
                const resp = await fetch(`${url}/health`);
                if (resp.ok) {
                    console.log(`[Test Pipeline] ${name} is healthy.`);
                    return;
                }
            } catch (e) {}
            await new Promise(r => setTimeout(r, 500));
        }
        throw new Error(`${name} failed to become healthy`);
    };

    await waitForHealth(`http://localhost:${PORT_OPS}`, 'Ops Node');
    await waitForHealth(`http://localhost:${PORT_EXPORT}`, 'Export Node');

    // 3. Setup JS Client Node (Peered with Export)
    console.log('[Test Pipeline] Starting JS Test Client...');
    clientVfs = new VFS({ id: 'pipeline-client', storage: new DiskStorage('.vfs_storage_pipeline-client') });
    mesh = new MeshLink(clientVfs, [`http://localhost:${PORT_EXPORT}`], { localUrl: `http://localhost:${PORT_CLIENT}` });
    clientServer = http.createServer();
    registerVFSRoutes(clientVfs, clientServer, '', mesh);
    await new Promise(resolve => clientServer.listen(PORT_CLIENT, '0.0.0.0', resolve));
    await clientVfs.init();
    await mesh.start();

    await t.test('should execute full structured pipeline across multiple language nodes', async () => {
        const pipeline = {
            path: 'op/export',
            parameters: {
                filename,
                source: {
                    path: 'op/pdf',
                    parameters: {
                        source: {
                            path: 'op/outline',
                            parameters: {
                                source: {
                                    path: 'op/offset',
                                    parameters: {
                                        radius: 5.0,
                                        source: {
                                            path: 'shape/hexagon/full',
                                            parameters: { radius: 50 }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        };

        console.log('[Test Pipeline] Requesting structured export...');
        const result = await clientVfs.readData(pipeline.path, pipeline.parameters);
        
        assert.ok(result, 'Export should return metadata');
        assert.ok(result.filename.includes(filename), 'Metadata should confirm filename');
        
        const stats = await fs.stat(path.resolve(filename));
        assert.ok(stats.size > 100, 'PDF file should be generated and non-empty');
        console.log(`[Test Pipeline] SUCCESS: Generated ${stats.size} byte PDF.`);
    });

    await t.test('should reject underconstrained request to C++ op', async () => {
        // First we need to make sure the client knows the schema.
        // In a real scenario, this happens via discovery (spy).
        // For the test, we'll manually add it to the client VFS.
        clientVfs.addSchema('shape/hexagon/full', {
            type: 'object',
            required: ['radius'],
            properties: { radius: { type: 'number' } }
        });

        console.log('[Test Pipeline] Requesting underconstrained hexagon (no radius)...');
        try {
            await clientVfs.read('shape/hexagon/full', {});
            assert.fail('Should have been rejected locally');
        } catch (err) {
            assert.ok(err.message.includes('Missing required parameter \'radius\''));
            console.log('[Test Pipeline] Local rejection SUCCESS:', err.message);
        }
    });

    await t.test('should reject stale request at C++ node', async () => {
        const past = Date.now() - 5000;
        console.log('[Test Pipeline] Sending raw stale request to C++ node...');
        
        const resp = await fetch(`http://localhost:${PORT_OPS}/read`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                path: 'shape/hexagon/full',
                parameters: { radius: 10 },
                expiresAt: past
            })
        });

        assert.strictEqual(resp.status, 404, 'C++ node should return 404 for expired request');
        console.log('[Test Pipeline] Remote C++ TTL rejection SUCCESS');
    });
});
