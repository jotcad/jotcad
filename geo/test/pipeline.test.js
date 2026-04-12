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
    let opsProcess, exportProcess, clientServer, clientVfs;
    const filename = 'pipeline_test_result.pdf';

    t.after(async () => {
        console.log('[Test Pipeline] Cleaning up...');
        if (opsProcess) opsProcess.kill();
        if (exportProcess) exportProcess.kill();
        if (clientServer) clientServer.close();
        if (clientVfs) await clientVfs.close();
        
        await fs.rm(path.resolve('.vfs_storage_pipeline-ops'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve('.vfs_storage_pipeline-export'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve('.vfs_storage_pipeline-client'), { recursive: true, force: true }).catch(() => {});
        await fs.rm(path.resolve(filename), { force: true }).catch(() => {});
    });

    // 1. Start C++ Ops Node (Leaf)
    console.log('[Test Pipeline] Launching C++ Ops Node...');
    opsProcess = spawn(path.resolve('geo/bin/ops'), [], {
        env: { ...process.env, PORT: PORT_OPS.toString(), PEER_ID: 'pipeline-ops' }
    });

    // 2. Start JS Export Node (Peered with Ops)
    console.log('[Test Pipeline] Launching JS Export Node...');
    exportProcess = spawn('node', [path.resolve('geo/src/export_service.js')], {
        env: { 
            ...process.env, 
            PORT: PORT_EXPORT.toString(), 
            PEER_ID: 'pipeline-export',
            NEIGHBORS: `http://localhost:${PORT_OPS}`
        }
    });

    await new Promise(resolve => setTimeout(resolve, 3000));

    // 3. Setup JS Client Node (Peered with Export)
    console.log('[Test Pipeline] Starting JS Test Client...');
    clientVfs = new VFS({ id: 'pipeline-client', storage: new DiskStorage('.vfs_storage_pipeline-client') });
    const mesh = new MeshLink(clientVfs, [`http://localhost:${PORT_EXPORT}`], { localUrl: `http://localhost:${PORT_CLIENT}` });
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
                                            path: 'shape/hexagon',
                                            parameters: { radius: 50, variant: 'full' }
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
});
