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
  getSelectorKey,
  Selector,
} from '../src/index.js';

import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const CPP_OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');
const PORT_CPP = 20301;
const PORT_JS = 20302;
const STORAGE_JS = path.resolve('.test_vfs_user_op_js');
const STORAGE_CPP = path.resolve('.vfs_storage_user_op_cpp');

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

test('User Operator Interaction Integration', { timeout: 120000 }, async (t) => {
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

  // 1. Start C++ Native Node
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

  // 2. Start JS Node
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

  // 3. Setup Compiler & Register Schemas Locally
  const compiler = new JotCompiler(jsVfs);
  const parser = new JotParser();

  const builtInOps = {
    'Hexagon': { path: 'jot/Hexagon/edgeToEdge', schema: { arguments: [{ name: 'edgeToEdge', type: 'number', default: 248 }, { name: 'turns', type: 'number', default: 0 }], outputs: { $out: 'shape' } } },
    'Disk': { path: 'jot/Disk', schema: { arguments: [{ name: 'diameter', type: 'interval', default: 10 }, { name: 'zag', type: 'number', default: 0.1 }], outputs: { $out: 'shape' } } },
    'corners': { path: 'jot/corners', schema: { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } } },
    'nth': { path: 'jot/nth', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'index', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'o': { path: 'jot/o', schema: { arguments: [{ name: '$in', type: 'shape', optional: true }], outputs: { $out: 'shape' } } },
    'by': { path: 'jot/by', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'targets', type: 'shapes' }], outputs: { $out: 'shape' } } },
    'clip': { path: 'jot/clip', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } },
    'ez': { path: 'jot/ez', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'height', type: 'interval' }], outputs: { $out: 'shape' } } },
    'cut': { path: 'jot/cut', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } },
    'mx': { path: 'jot/mx', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'x', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'mz': { path: 'jot/mz', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'z', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'rx': { path: 'jot/rx', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'ry': { path: 'jot/ry', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'rz': { path: 'jot/rz', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'pdf': { path: 'jot/pdf', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'path', type: 'string' }], outputs: { $out: 'shape', file: 'file' } } }
  };

  for (const [name, def] of Object.entries(builtInOps)) {
    compiler.registerOperator(name, def);
  }

  const footScript = `
H = $in
C = H.corners().nth(0)
H .by(C.o())
  .clip(Disk(40))
  .ez(10)
  .cut(mx(4).mz(-4))
  .cut(Disk(5.1).ez(50).rx(1/4).mx(10).rz(1/6, -1/6))
  .cut(Disk(1.4).mx(10).ez([0, 20]))
  .ry(1/2) -> $out
  `;
  const footSchema = {
    path: 'user/Foot',
    arguments: [{ name: '$in', type: 'jot:shape' }],
    outputs: { '$out': { type: 'jot:shape' } }
  };

  const testScript = `
Hexagon(edgeToEdge=248).Foot().pdf('hex_foot2.pdf').file -> $hex
  `;
  const testSchema = {
    path: 'user/Test',
    arguments: [],
    outputs: { '$hex': { type: 'file', mimeType: 'application/pdf' } }
  };

  compiler.registerOperator('user/Foot', { path: 'user/Foot', script: footScript, schema: footSchema });
  compiler.registerOperator('user/Test', { path: 'user/Test', script: testScript, schema: testSchema });

  // 4. VFS Providers
  jsVfs.registerProvider('user/Foot', async (vfs, selector) => {
    console.log('[Test] user/Foot evaluation start');
    const ast = parser.parse(footScript);
    const results = await compiler.evaluate(ast, selector.parameters, footSchema, 'user/Foot');
    console.log('[Test] user/Foot results:', JSON.stringify(results, null, 2));
    if (!results || results.length === 0) throw new Error('user/Foot failed to produce output');
    return await vfs.read(results[0].selector);
  });

  jsVfs.registerProvider('user/Test', async (vfs, selector) => {
    console.log('[Test] user/Test evaluation start');
    const ast = parser.parse(testScript);
    const results = await compiler.evaluate(ast, selector.parameters, testSchema, 'user/Test');
    console.log('[Test] user/Test results:', JSON.stringify(results, null, 2));
    const hexResult = results.find(r => r.port === '$hex');
    if (!hexResult) throw new Error('user/Test failed to produce $hex output');
    return await vfs.read(hexResult.selector);
  });

  // 5. Execution
  console.log('[Test] Requesting user/Test output...');
  try {
    const { stream, metadata } = await jsVfs.read(new Selector('user/Test').withOutput('$hex'));
    console.log('[Test] Metadata:', metadata);
    assert.strictEqual(metadata.state, 'AVAILABLE');
    
    const pdfData = await consumeBytes(stream);
    console.log('[Test] PDF length:', pdfData.length);
    assert.ok(pdfData.length > 0, 'PDF should not be empty');
    assert.strictEqual(new TextDecoder().decode(pdfData.slice(0, 5)), '%PDF-', 'Valid PDF header');
  } catch (err) {
    console.error('[Test] FAIL:', err);
    throw err;
  }
});
