import test from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import http from 'node:http';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

import {
  VFS,
  MeshLink,
  registerVFSRoutes,
  DiskStorage,
  Selector,
} from '../src/index.js';

import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const CPP_OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');
const PORT_CPP = 20401;
const PORT_JS = 20402;
const STORAGE_JS = path.resolve('.test_vfs_multi_out_js');
const STORAGE_CPP = path.resolve('.vfs_storage_multi_out_cpp');

async function consumeBytes(stream) {
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
    return bytes;
}

test('Multi-Output Block Syntax Integration', { timeout: 120000 }, async (t) => {
  let cppNode;
  let server;
  let jsVfs;
  let stopServer;

  t.after(async () => {
    if (cppNode) cppNode.kill();
    if (stopServer) stopServer();
    if (server) server.close();
    if (jsVfs) await jsVfs.close();
    await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
    await fs.rm(STORAGE_CPP, { recursive: true, force: true }).catch(() => {});
  });

  // 1. Start C++ Native Node (Provides Hexagon and PDF)
  cppNode = spawn(CPP_OPS_PATH, [PORT_CPP.toString(), STORAGE_CPP], {
    env: { ...process.env, PEER_ID: 'cpp-node' },
    stdio: 'inherit',
  });

  let healthy = false;
  for (let i = 0; i < 20; i++) {
    try {
      const resp = await fetch(`http://localhost:${PORT_CPP}/health`);
      if (resp.ok) { healthy = true; break; }
    } catch (e) {}
    await new Promise(r => setTimeout(r, 500));
  }
  if (!healthy) throw new Error('C++ Node failed to start');

  // 2. Start JS Node (Orchestrator)
  jsVfs = new VFS({
    id: 'js-node',
    storage: new DiskStorage(STORAGE_JS),
  });
  const mesh = new MeshLink(jsVfs, [`http://localhost:${PORT_CPP}`], {
    localUrl: `http://localhost:${PORT_JS}`,
  });
  server = http.createServer();
  stopServer = registerVFSRoutes(jsVfs, server, '', mesh);
  await new Promise(r => server.listen(PORT_JS, '0.0.0.0', r));
  await jsVfs.init();
  await mesh.start();

  // 3. Setup Compiler & Register Schemas
  const compiler = new JotCompiler(jsVfs);
  const parser = new JotParser();

  // Built-in schemas (matching what C++ node provides)
  compiler.registerOperator('Hexagon', { 
    path: 'jot/Hexagon/edgeToEdge', 
    schema: { 
        arguments: [{ name: 'edgeToEdge', type: 'number', default: 10 }], 
        outputs: { $out: 'shape' } 
    } 
  });
  
  compiler.registerOperator('pdf', { 
    path: 'jot/pdf', 
    schema: { 
        arguments: [{ name: '$in', type: 'shape' }, { name: 'path', type: 'string' }], 
        outputs: { $out: 'file' } 
    } 
  });

  // 4. Define the Multi-Output User Operator
  const userOpPath = 'user/HexWithPDF';
  const userOpScript = `Hexagon(edgeToEdge=50).{
  pdf('export.pdf') -> $pdf
} -> $out`;
  
  const userOpSchema = {
    path: userOpPath,
    arguments: [],
    outputs: { 
        '$out': { type: 'shape' },
        '$pdf': { type: 'file', mimeType: 'application/pdf' }
    }
  };

  compiler.registerOperator(userOpPath, { path: userOpPath, script: userOpScript, schema: userOpSchema });

  jsVfs.registerProvider(userOpPath, async (vfs, selector) => {
    console.log('[Test] user/HexWithPDF evaluation start');
    const ast = parser.parse(userOpScript);
    const results = await compiler.evaluate(ast, selector.parameters, userOpSchema, userOpPath);
    
    let requestedTerminal = null;
    const requestedPort = selector.output || '$out';

    for (const bundle of results) {
        if (bundle.port === requestedPort) {
            requestedTerminal = bundle.selector;
        } else {
            // CRITICAL: Link secondary outputs to the VFS so they can be read by remote nodes
            await vfs.link(selector.withOutput(bundle.port), bundle.selector);
        }
    }
    return requestedTerminal;
  }, { schema: userOpSchema });

  // 5. Verify Fulfillment
  
  await t.test('Fulfill $out (The Hexagon)', async () => {
    const { stream, metadata } = await jsVfs.read(new Selector(userOpPath).withOutput('$out'));
    assert.strictEqual(metadata.state, 'AVAILABLE');
    const data = await consumeBytes(stream);
    const shape = JSON.parse(new TextDecoder().decode(data));
    assert.ok(shape.geometry, 'Should have geometry');
  });

  await t.test('Fulfill $pdf (The Side-effect)', async () => {
    const { stream, metadata } = await jsVfs.read(new Selector(userOpPath).withOutput('$pdf'));
    assert.strictEqual(metadata.state, 'AVAILABLE');
    const pdfData = await consumeBytes(stream);
    assert.strictEqual(new TextDecoder().decode(pdfData.slice(0, 5)), '%PDF-', 'Valid PDF header');
  });
});
