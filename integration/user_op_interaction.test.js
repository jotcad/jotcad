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

test('User Operator Interaction Integration', { timeout: 120000 }, async (t) => {
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
  sys = await launchSystem(PROFILES.TEST);
  const PORT_CPP = sys.ports.ops;
  const PORT_JS = 20302;

  // 2. Start JS Node
  jsVfs = new VFS({
    id: 'js-node',
    storage: new DiskStorage('.vfs_storage_user_op_js'),
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
    'corners': { path: 'jot/corners', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [], outputs: { $out: 'shape' } } },
    'nth': { path: 'jot/nth', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'index', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'o': { path: 'jot/o', schema: { inputs: { '$in': { type: 'shape', optional: true } }, arguments: [], outputs: { $out: 'shape' } } },
    'by': { path: 'jot/by', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'targets', type: 'shapes' }], outputs: { $out: 'shape' } } },
    'clip': { path: 'jot/clip', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } },
    'ez': { path: 'jot/ez', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'height', type: 'interval' }], outputs: { $out: 'shape' } } },
    'cut': { path: 'jot/cut', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'other', type: 'shape' }], outputs: { $out: 'shape' } } },
    'mx': { path: 'jot/mx', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'x', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'mz': { path: 'jot/mz', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'z', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'rx': { path: 'jot/rx', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'ry': { path: 'jot/ry', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'rz': { path: 'jot/rz', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'turns', type: 'numbers' }], outputs: { $out: 'shape' } } },
    'pdf': { path: 'jot/pdf', schema: { inputs: { '$in': { type: 'shape' } }, arguments: [{ name: 'path', type: 'string' }], outputs: { $out: 'file' } } }
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
    inputs: { '$in': { type: 'jot:shape' } },
    arguments: [],
    outputs: { '$out': { type: 'jot:shape' } }
  };

  const testScript = `
Hexagon(edgeToEdge=248).Foot().pdf('hex_foot2.pdf') -> $hex
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
    const ast = parser.parse(footScript);
    const results = await compiler.evaluate(ast, selector.parameters, footSchema, 'user/Foot');
    if (!results || results.length === 0) throw new Error('user/Foot failed to produce output');
    return await vfs.read(results[0].selector);
  });

  jsVfs.registerProvider('user/Test', async (vfs, selector) => {
    const ast = parser.parse(testScript);
    const results = await compiler.evaluate(ast, selector.parameters, testSchema, 'user/Test');
    const hexResult = results.find(r => r.port === '$hex');
    if (!hexResult) throw new Error('user/Test failed to produce $hex output');
    return await vfs.read(hexResult.selector);
  });

  // 5. Execution
  console.log('[Test] Requesting user/Test output...');
  try {
    const { stream, metadata } = await jsVfs.read(new Selector('user/Test').withOutput('$hex'));
    assert.strictEqual(metadata.state, 'AVAILABLE');
    
    const pdfData = await consumeBytes(stream);
    assert.ok(pdfData.length > 0, 'PDF should not be empty');
    assert.strictEqual(new TextDecoder().decode(pdfData.slice(0, 5)), '%PDF-', 'Valid PDF header');
  } catch (err) {
    console.error('[Test] FAIL:', err);
    throw err;
  }
});
