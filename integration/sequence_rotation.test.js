import test from 'node:test';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import assert from 'node:assert/strict';
import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
} from '../fs/src/index.js';

import { JotCompiler } from '../jot/src/compiler.js';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';

async function consumeJSON(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('Sequence Rotation and Section Integration Tests', { timeout: 120000 }, async (t) => {
  let sys;
  let server;
  let jsVfs;
  let stopServer;

  const PORT_JS = 20108;
  const STORAGE_JS = path.resolve('.test_vfs_seq_rot_js');

  t.after(async () => {
    if (stopServer) stopServer();
    if (server) server.close();
    if (jsVfs) await jsVfs.close();
    if (sys) await sys.stop();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
  });

  // 1. Launch the TEST system
  sys = await launchSystem(PROFILES.TEST);
  const PORT_CPP = sys.ports.ops;

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

  // 3. Register remote operators in compiler
  const compiler = new JotCompiler(jsVfs);
  const catalogUrl = `http://localhost:${PORT_CPP}/catalog`;
  console.log(`[Test] Fetching catalog from ${catalogUrl}`);
  const resp = await fetch(catalogUrl);
  if (!resp.ok) {
      const text = await resp.text();
      console.error(`[Test] Catalog fetch failed (${resp.status}): ${text}`);
      throw new Error(`Catalog fetch failed: ${resp.status}`);
  }
  const catalog = await resp.json();
  console.log(`[Test] Catalog loaded with ${Object.keys(catalog.catalog).length} operators`);
  
  for (const [name, schema] of Object.entries(catalog.catalog)) {
      compiler.registerOperator(name, { path: name, schema: schema });
  }

  await t.test('Box(1, 10).rz([by 1/8]).fuse()', async () => {
    const code = "Box(1, 10).rz([by 1/8]).fuse() -> $out";
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Read and verify the shape metadata
        const shape = await consumeJSON((await jsVfs.read(selector)).stream);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of the fused shape
        await captureAndVerifyPNG(jsVfs, selector, 'sequence_rotation_fuse_result.png');
        console.log("SUCCESS: Box sequence rotation and fuse integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 1).rz([by 1/8]).section()', async () => {
    const code = "Box(1, 10, 1).rz([by 1/8]).section() -> $out";
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Read and verify the shape metadata
        const shape = await consumeJSON((await jsVfs.read(selector)).stream);
        console.log("Shape tags:", shape.tags);
        assert.strictEqual(shape.tags.type, 'group', "Expected result tag type to be 'group'");
        assert.ok(shape.components && shape.components.length > 0, "Expected result to be a group with child components");
        assert.strictEqual(shape.components[0].tags.type, 'surface', "Expected child components to have 'surface' tag type");
        assert.ok(shape.components[0].geometry, "Expected child components to have a geometry CID");

        // Save PNG capture of the sectioned shape
        await captureAndVerifyPNG(jsVfs, selector, 'sequence_rotation_section_result.png');
        console.log("SUCCESS: Box sequence rotation and section integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10).rz([by 1/8]).outline().fill(rule="odd")', async () => {
    const code = 'Box(1, 10).rz([by 1/8]).outline().fill(rule="odd") -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Read and verify the shape metadata
        const shape = await consumeJSON((await jsVfs.read(selector)).stream);
        console.log("Shape tags:", shape.tags);
        assert.strictEqual(shape.tags.type, 'surface', "Expected result tag type to be 'surface'");
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of the filled shape
        await captureAndVerifyPNG(jsVfs, selector, 'sequence_rotation_fill_result.png');
        console.log("SUCCESS: Box sequence rotation outline and fill integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 2).rz([by 1/8]).section().fuse().ez(5)', async () => {
    const code = 'Box(1, 10, 2).rz([by 1/8]).section().fuse().ez(5) -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Read and verify the shape metadata
        const shape = await consumeJSON((await jsVfs.read(selector)).stream);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of this shape
        await captureAndVerifyPNG(jsVfs, selector, 'sequence_rotation_extruded_fuse_result.png');
        console.log("SUCCESS: Box sequence rotation section fuse and extrude test completed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 2).rz([by 1/8]).section().fuse().outline()', async () => {
    const code = 'Box(1, 10, 2).rz([by 1/8]).section().fuse().outline() -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const parser = new (await import('../jot/src/parser.js')).JotParser();
        const ast = parser.parse(code);
        const results = await compiler.evaluate(ast, {}, { outputs: { '$out': 'jot:shape' } });
        
        const selector = results[0].selector;
        console.log(`[Test] Result Selector: ${JSON.stringify(selector, null, 2)}`);

        // Read and verify the shape metadata
        const shape = await consumeJSON((await jsVfs.read(selector)).stream);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of this shape
        await captureAndVerifyPNG(jsVfs, selector, 'sequence_rotation_outline_result.png');
        console.log("SUCCESS: Box sequence rotation section fuse and outline test completed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });
});
