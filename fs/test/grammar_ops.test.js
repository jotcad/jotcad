import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';

const OPS_PORT = 9200;
const OPS_URL = `http://localhost:${OPS_PORT}`;
const STORAGE_OPS = path.resolve('.vfs_storage_grammar-ops');
const STORAGE_JS = path.resolve('.vfs_storage_grammar-js');

test('Geometric Grammar Integration', async (t) => {
    let opsProcess;
    let vfs;
    let mesh;
    let server;

    t.after(async () => {
        console.log('[Test Grammar] Cleaning up...');
        if (opsProcess) opsProcess.kill();
        if (server) server.close();
        if (vfs) await vfs.close();
        
        await fs.rm(STORAGE_OPS, { recursive: true, force: true }).catch(() => {});
        await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
    });

    // 1. Start Native C++ Node
    console.log('[Test Grammar] Launching C++ Native Node...');
    opsProcess = spawn(path.resolve('geo/bin/ops'), [], {
        env: { 
            ...process.env, 
            PORT: OPS_PORT.toString(), 
            PEER_ID: 'grammar-ops-node',
            NEIGHBORS: '' 
        }
    });

    // opsProcess.stdout.on('data', (d) => console.log(`[C++ stdout] ${d}`));

    // Wait for C++ node to be ready
    await new Promise(resolve => setTimeout(resolve, 2000));

    // 2. Start JS Node
    console.log('[Test Grammar] Starting Node.js Test Client...');
    vfs = new VFS({ id: 'grammar-js-node', storage: new DiskStorage(STORAGE_JS) });
    mesh = new MeshLink(vfs, [OPS_URL], { localUrl: 'http://localhost:9201' });
    server = http.createServer();
    registerVFSRoutes(vfs, server, '', mesh);
    
    await new Promise(resolve => server.listen(9201, '0.0.0.0', resolve));
    await vfs.init();
    await mesh.start();

    await t.test('should construct a hexagon sector using nested mesh operations', async () => {
        // THE EXPRESSION:
        // loop(group(origin, nth(points(hexagon), [0, 1])))
        
        const hexagon = { path: 'shape/hexagon', parameters: { radius: 100 } };
        const points = { path: 'op/points', parameters: { source: hexagon } };
        const firstTwo = { path: 'op/nth', parameters: { source: points, indices: [0, 1] } };
        const origin = { path: 'shape/origin', parameters: {} };
        const group = { path: 'op/group', parameters: { sources: [origin, firstTwo] } };
        const sector = { path: 'op/loop', parameters: { source: group } };

        console.log('[Test Grammar] Requesting complex grammar sector...');
        const geoText = await vfs.readData(sector.path, sector.parameters);
        
        assert.ok(geoText, 'Result should be defined');
        
        // Validation:
        const lines = geoText.trim().split('\n');
        const vCount = lines.filter(l => l.startsWith('v ')).length;
        const lCount = lines.filter(l => l.startsWith('l ')).length;

        console.log(`[Test Grammar] Result has ${vCount} vertices and ${lCount} segments.`);
        
        assert.strictEqual(vCount, 3, 'Should have 3 vertices (origin + 2 hex points)');
        assert.strictEqual(lCount, 3, 'Should have 3 segments (triangle)');
        
        // Origin should be the first vertex
        assert.ok(lines[0].includes('v 0.000000 0.000000 0.000000'), 'First vertex should be origin');
    });
});
