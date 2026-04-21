import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';
import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
  getSelectorKey,
} from '../src/index.js';

import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const CPP_OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');
const PORT_CPP = 20101;
const PORT_JS = 20102;
const STORAGE_JS = path.resolve('.test_vfs_cpp_integration_js');
const STORAGE_CPP = path.resolve('.vfs_storage_cpp-test-node');

test('C++ Native Node Integration', { timeout: 30000 }, async (t) => {
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
  cppNode = spawn(CPP_OPS_PATH, [PORT_CPP.toString(), STORAGE_CPP], {
    env: {
      ...process.env,
      PEER_ID: 'cpp-test-node',
    },
    stdio: 'inherit',
  });

  // Wait for C++ node to be healthy
  let healthy = false;
  for (let i = 0; i < 10; i++) {
    try {
      const resp = await fetch(`http://localhost:${PORT_CPP}/health`);
      if (resp.ok) {
        healthy = true;
        break;
      }
    } catch (e) {}
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  if (!healthy) throw new Error('C++ Node failed to start');

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-test-client',
    storage: new DiskStorage(STORAGE_JS),
  });
  const mesh = new MeshLink(jsVfs, [`http://localhost:${PORT_CPP}`], {
    localUrl: `http://localhost:${PORT_JS}`,
  });
  server = http.createServer();
  stopServer = registerVFSRoutes(jsVfs, server, '', mesh);

  await new Promise((resolve) => server.listen(PORT_JS, '0.0.0.0', resolve));
  await jsVfs.init();
  await mesh.start();

  await t.test('Health Check', async () => {
    const resp = await fetch(`http://localhost:${PORT_CPP}/health`);
    const info = await resp.json();
    assert.strictEqual(info.status, 'OK');
  });

  await t.test('Peer Registration (Exercises probeDirectReachability)', async () => {
    const resp = await fetch(`http://localhost:${PORT_JS}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: 'cpp-test-node', url: `http://localhost:${PORT_CPP}` })
    });
    assert.strictEqual(resp.status, 200, 'JS node should accept registration from C++ node');
    const info = await resp.json();
    assert.strictEqual(info.id, 'js-test-client');
  });

  await t.test('CID Consistency', async () => {
    // Note: box op with these params produces a deterministic CID
    const selector = { path: 'jot/Box', parameters: { depth: 0, height: 10, width: 10 } };
    const jsAddrKey = await getSelectorKey(selector);
    await jsVfs.readData(selector);

    // In Content-Addressable Mesh:
    // 1. The selector hash identifies the .meta file (Address Key)
    const metaFile = path.join(STORAGE_CPP, `${jsAddrKey}.meta`);
    await fs.access(metaFile); 
    
    const meta = JSON.parse(await fs.readFile(metaFile, 'utf8'));
    assert.ok(meta.cid, 'Meta file should contain a content CID');
    
    // 2. The .meta file points to the .data file (Content Identity)
    const dataFile = path.join(STORAGE_CPP, `${meta.cid}.data`);
    await fs.access(dataFile);
  });

  await t.test('Provisioning: Box', async () => {
    const result = await jsVfs.readData({ path: 'jot/Box', parameters: {
      width: 20,
      height: 20,
      depth: 0,
    } });
    assert.ok(result, 'Box provisioning should return a Shape');
    assert.strictEqual(typeof result, 'object', 'Result should be an object (Shape)');
    assert.strictEqual(result.tags?.type, 'box');
    const geo = await jsVfs.readData(result.geometry);
    const geoText = new TextDecoder().decode(geo);
    assert.ok(geoText.includes('v 10.000000'), 'Should contain x coordinate');
  });

  await t.test('Provisioning: Triangle', async () => {
    const result = await jsVfs.readData({ path: 'jot/Triangle/equilateral', parameters: {
      size: [50],
    } });
    assert.ok(result, 'Triangle provisioning should return a Shape');
    assert.strictEqual(typeof result, 'object', 'Result should be an object (Shape)');
    assert.strictEqual(result.tags?.type, 'triangle');
  });
});
