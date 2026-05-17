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
} from '../fs/src/index.js';

import { vfsResult } from './vfs_test_helpers.js';


import { JotParser } from '../jot/src/parser.js';
import { JotCompiler } from '../jot/src/compiler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const CPP_OPS_PATH = path.resolve(__dirname, '../geo/bin/ops');
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

test('Multi-Output Scoped Block Integration', { timeout: 60000 }, async (t) => {
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

  compiler.registerOperator('Box', { path: 'jot/Box', schema: { arguments: [{ name: 'width', type: 'number', default: 10 }, { name: 'height', type: 'number', default: 10 }, { name: 'depth', type: 'number', default: 0 }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('Disk', { path: 'jot/Disk', schema: { arguments: [{ name: 'diameter', type: 'number', default: 10 }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('cut', { path: 'jot/cut', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('mx', { path: 'jot/mx', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'x', type: 'numbers' }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('mz', { path: 'jot/mz', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'z', type: 'numbers' }], outputs: { $out: 'shape' } } });

  await t.test('Scoped block with multiple outputs correctly resolves ports', async () => {
    // SCRIPT:
    // B = Box(20)
    // D = Disk(10)
    // B.cut(D) -> $out
    // D.mx(5) -> extra
    const script = 'B = Box(20); D = Disk(10); B.cut(D) -> $out; D.mx(5) -> extra';
    const ast = parser.parse(script);

    console.log('[Test] Evaluating block script...');
    const terminals = await compiler.evaluate(ast, {}, {
        outputs: {
            "$out": { type: "jot:shape" },
            "extra": { type: "jot:shape" }
        }
    });

    assert.strictEqual(terminals.length, 2, 'Should have 2 terminal outputs from block');
    
    const outResult = terminals.find(t => t.port === '$out');
    const extraResult = terminals.find(t => t.port === 'extra');

    assert.ok(outResult, 'Should find $out terminal');
    assert.ok(extraResult, 'Should find extra terminal');

    assert.strictEqual(outResult.selector.path, 'jot/cut', '$out should be the result of the cut op');
    assert.strictEqual(extraResult.selector.path, 'jot/mx', 'extra should be the result of the mx op');

    // Verify retrieval of both
    console.log('[Test] Requesting $out from VFS...');
    const { stream: s1 } = await jsVfs.read(outResult.selector);
    const shape1 = JSON.parse(new TextDecoder().decode(await consumeBytes(s1)));
    assert.strictEqual(shape1.tags.type, 'surface');

    console.log('[Test] Requesting extra from VFS...');
    const { stream: s2 } = await jsVfs.read(extraResult.selector);
    const shape2 = JSON.parse(new TextDecoder().decode(await consumeBytes(s2)));
    assert.strictEqual(shape2.tags.type, 'surface');
  });

  await t.test('Sub-selector port resolution across mesh', async () => {
    // Defining an op that returns multiple ports
    const opSchema = {
        path: 'user/MultiOut',
        arguments: [],
        outputs: {
            'main': { type: 'jot:shape' },
            'side': { type: 'jot:shape' }
        }
    };

    jsVfs.registerProvider('user/MultiOut', async (v, s) => {
        const script = 'Box(10) -> main; Disk(5) -> side';
        const results = await compiler.evaluate(parser.parse(script), {}, opSchema);
        const target = results.find(r => r.port === s.output);
        // Inject a tag to verify we got the right port
        const res = await v.read(target.selector);
        const shape = JSON.parse(new TextDecoder().decode(await consumeBytes(res.stream)));
        shape.tags.port = s.output;
        return vfsResult(shape, res.metadata);
    }, { schema: opSchema });

    console.log('[Test] Requesting user/MultiOut:side...');
    const sideSelector = new Selector('user/MultiOut').withOutput('side');
    const { stream } = await jsVfs.read(sideSelector);
    const shape = JSON.parse(new TextDecoder().decode(await consumeBytes(stream)));
    
    console.log('[Test] Side port shape:', JSON.stringify(shape, null, 2));
    
    assert.strictEqual(shape.tags.port, 'side', 'Result should be from the requested side port');
    assert.strictEqual(shape.tags.type, 'surface', 'Result should be a surface (Disk)');
  });
});
