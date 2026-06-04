import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';
import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
  Selector,
} from '../fs/src/index.js';

import { launchSystem, PROFILES } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';

test('Texture Mesh Resolution Integration', { timeout: 60000 }, async (t) => {
  let sys;
  let server;
  let jsVfs;
  let stopServer;

  const PORT_JS = 20102;
  const STORAGE_JS = path.resolve('.test_vfs_texture_integration_js');

  t.after(async () => {
    if (stopServer) stopServer();
    if (server) server.close();
    if (jsVfs) await jsVfs.close();
    if (sys) await sys.stop();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
  });

  // 1. Launch the TEST system (C++ Kernel)
  const profileKey = 'test/standard';
  sys = await launchSystem(profileKey);
  const PORT_CPP = sys.ports.ops;

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-texture-provider',
    storage: new DiskStorage(STORAGE_JS),
  });

  // Register a mock jot/texture provider
  jsVfs.registerProvider('jot/texture', async (v, s) => {
    const material = s.parameters.material;
    if (material === 'test_pattern') {
      // 2x2 tiny checkboard PNG
      const pngBase64 = 'iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAFklEQVQI12P4z8DAwMDAxMDAwMDAAAAMAgEAKh2GjQAAAABJRU5ErkJggg==';
      const bytes = new Uint8Array(Buffer.from(pngBase64, 'base64'));
      return {
          stream: new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          }),
          metadata: { contentType: 'image/png' }
      };
    }
    return null;
  });

  const mesh = new MeshLink(jsVfs, [`http://localhost:${PORT_CPP}`], {
    localUrl: `http://localhost:${PORT_JS}`,
  });
  server = http.createServer();
  stopServer = registerVFSRoutes(jsVfs, server, '', mesh);

  await new Promise((resolve) => server.listen(PORT_JS, '0.0.0.0', resolve));
  await jsVfs.init();
  await mesh.start();

  await t.test('Peer Registration', async () => {
    const resp = await fetch(`http://localhost:${PORT_JS}/register`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: 'geo-ops-node', url: `http://localhost:${PORT_CPP}` })
    });
    assert.strictEqual(resp.status, 200);
  });

  await t.test('Material to Texture Resolution (End to End)', async () => {
    // 1. Create a shape with a material tag
    const matSelector = new Selector('jot/material', {
        $in: new Selector('jot/Box', { width: 10, height: 10, depth: 0 }).withOutput('$out'),
        material: 'test_pattern'
    }).withOutput('$out');
    
    // 2. Render it via C++. The C++ kernel should request 'jot/texture?material=test_pattern' over the mesh, 
    //    our JS Node will fulfill it, and C++ will render it.
    console.log('[Test] Initiating PNG render with Texture resolution...');
    const pngBytes = await captureAndVerifyPNG(jsVfs, matSelector, 'textured_box_test.png');
    
    // Minimal check: it's a valid PNG and not empty. Visual verification happens via the artifact.
    assert.ok(pngBytes.length > 100);
    assert.strictEqual(pngBytes[0], 0x89);
  });
});
