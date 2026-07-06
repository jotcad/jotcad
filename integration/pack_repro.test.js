import test from 'node:test';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
  Selector,
} from '../fs/src/index.js';

import { JotCompiler } from '../jot/src/compiler.js';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';

test('Pack Repro: Triangle(20).dup(1).pack(sheet=Box(30,30))', { timeout: 60000 }, async (t) => {
  let sys;
  let server;
  let jsVfs;
  let stopServer;
  let mesh;

  const PORT_JS = 20105;
  const STORAGE_JS = path.resolve('.test_vfs_pack_repro_js');

  t.after(async () => {
    if (stopServer) stopServer();
    if (server) await new Promise(resolve => server.close(resolve));
    if (mesh) {
        await Promise.race([
            mesh.stop(),
            new Promise((resolve) => setTimeout(resolve, 1000))
        ]);
    }
    if (jsVfs) await jsVfs.close();
    if (sys) await sys.stop();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
  });

  // 1. Launch the TEST system
  sys = await launchSystem('test/standard');
  const PORT_CPP = sys.ports.ops;

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-test-client',
    storage: new DiskStorage(STORAGE_JS),
  });
  mesh = new MeshLink(jsVfs, [`http://localhost:${sys.ports.zenoh_router}`], {
    localUrl: `http://localhost:${PORT_JS}`,
  });
  server = http.createServer();
  stopServer = registerVFSRoutes(jsVfs, server, '', mesh);

  await new Promise((resolve) => server.listen(PORT_JS, '0.0.0.0', resolve));
  await jsVfs.init();
  await mesh.start();

  // 3. Register remote operators in compiler
  const compiler = new JotCompiler(jsVfs);
  console.log("Waiting for geo-ops-node to connect...");
  const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
  await waitForMeshNodes(jsVfs, ['geo-ops-node']);

  console.log("Subscribing to sys/schema for catalog discovery...");
  let catalogReceived = null;
  jsVfs.events.on('notify', (selector, payload) => {
      if (selector.path === 'sys/schema') {
          catalogReceived = payload;
      }
  });

  await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);

  let attempts = 0;
  while (!catalogReceived && attempts < 50) {
      await new Promise(r => setTimeout(r, 100));
      attempts++;
  }

  if (!catalogReceived) {
      throw new Error('Should have received schema catalog');
  }
  const catalog = catalogReceived.catalog;
  console.log(`[Test] Catalog loaded with ${Object.keys(catalog).length} operators`);
  for (const [name, schema] of Object.entries(catalog)) {
      compiler.registerOperator(name, { path: name, schema: schema });
  }

  await t.test('Reproduction Case', async () => {
    const code = "Triangle(20).color('blue').dup(20).pack(sheet=Disk(100).color('red'), rotations=2) -> $out";
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Use helper for PNG generation and verification
        const EXPECTED_HASH = '419b5d2ca3006046b102ebe7de26856b41cef24ab1118f960d2357e8cccb1389';
        await captureAndVerifyPNG(jsVfs, selector, 'pack_repro_result.png', EXPECTED_HASH);
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });
});
