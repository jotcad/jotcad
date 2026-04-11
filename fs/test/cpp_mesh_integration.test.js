import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage, getCID } from '../src/index.js';

const CPP_OPS_PATH = path.resolve('geo/bin/ops');
const PORT_CPP = 9501;
const PORT_JS = 9502;
const STORAGE_JS = path.resolve('.test_vfs_cpp_integration_js');
const STORAGE_CPP = path.resolve('.vfs_storage_cpp-test-node');

test('C++ Native Node Integration', async (t) => {
    let cppNode;
    let server;
    let jsVfs;
    let stopServer;

    t.after(async () => {
        console.log('[Test] Cleaning up...');
        if (cppNode) cppNode.kill();
        if (stopServer) stopServer();
        if (server) server.close();
        if (jsVfs) await jsVfs.close();
        
        await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
        await fs.rm(STORAGE_CPP, { recursive: true, force: true }).catch(() => {});
    });

    // 1. Start C++ Native Node
    cppNode = spawn(CPP_OPS_PATH, [], {
        env: { ...process.env, PORT: PORT_CPP.toString(), PEER_ID: 'cpp-test-node' }
    });

    await new Promise(resolve => setTimeout(resolve, 2000));

    // 2. Start JS Node
    jsVfs = new VFS({ id: 'js-test-client', storage: new DiskStorage(STORAGE_JS) });
    const mesh = new MeshLink(jsVfs, [`http://localhost:${PORT_CPP}`], { localUrl: `http://localhost:${PORT_JS}` });
    server = http.createServer();
    stopServer = registerVFSRoutes(jsVfs, server, '', mesh);
    
    await new Promise(resolve => server.listen(PORT_JS, '0.0.0.0', resolve));
    await jsVfs.init();
    await mesh.start();

    await t.test('Health Check', async () => {
        const resp = await fetch(`http://localhost:${PORT_CPP}/health`);
        const info = await resp.json();
        assert.strictEqual(info.status, 'OK');
    });

    await t.test('CID Consistency', async () => {
        const selector = { path: 'shape/origin', parameters: {} };
        const jsCid = await getCID(selector);
        await jsVfs.readData(selector.path, selector.parameters);
        
        const dataFile = path.join(STORAGE_CPP, `${jsCid}.data`);
        await fs.access(dataFile); // Throws if CID doesn't match
    });

    await t.test('Provisioning: Box', async () => {
        const result = await jsVfs.readData('shape/box', { width: 10, height: 20, depth: 5 });
        assert.strictEqual(result.tags.type, 'box');
        const geo = await jsVfs.readData('geo/mesh', { width: 10, height: 20, depth: 5 });
        assert.ok(geo.includes('v 10.000000'), 'Should contain x coordinate');
        assert.ok(geo.includes('20.000000'), 'Should contain y coordinate');
        assert.ok(geo.includes('5.000000'), 'Should contain z coordinate');
    });

    await t.test('Provisioning: Triangle', async () => {
        const result = await jsVfs.readData('shape/triangle', { form: 'equilateral', side: 50 });
        assert.strictEqual(result.tags.type, 'triangle');
    });
});
