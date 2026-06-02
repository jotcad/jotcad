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

import { vfsResult } from './vfs_test_helpers.js';
import { JotParser } from '../jot/src/parser.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { launchSystem, PROFILES } from '../orchestrator.js';

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
  let sys;
  let server;
  let jsVfs;
  let stopServer;

  t.after(async () => {
    if (stopServer) stopServer();
    if (server) server.close();
    if (jsVfs) await jsVfs.close();
    if (sys) await sys.stop();
  });

  // 1. Launch the TEST system
  sys = await launchSystem('test/standard');
  const PORT_CPP = sys.ports.ops;
  const PORT_JS = 20402;

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-node',
    storage: new DiskStorage('.vfs_storage_multi_out_js'),
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
  compiler.registerOperator('cut', { path: 'jot/cut', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('mx', { path: 'jot/mx', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'x', type: 'numbers' }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('mz', { path: 'jot/mz', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'z', type: 'numbers' }], outputs: { $out: 'shape' } } });

  await t.test('Scoped block with multiple outputs correctly resolves ports', async () => {
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
    
    assert.strictEqual(shape.tags.port, 'side', 'Result should be from the requested side port');
    assert.strictEqual(shape.tags.type, 'surface', 'Result should be a surface (Disk)');
  });
});
