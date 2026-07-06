import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, MemoryStorage, Selector } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';

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

test('Mold Generation & Analysis - JOT Expression Integration', { timeout: 60000 }, async (t) => {
  let sys;
  let vfs;
  let mesh;

  t.after(async () => {
    if (mesh) {
        await Promise.race([
            await mesh.stop(),
            new Promise((resolve) => setTimeout(resolve, 1000))
        ]);
    }
    if (vfs) await vfs.close();
    if (sys) await sys.stop();
  });

  // 1. Launch the TEST system
  sys = await launchSystem('test/standard');
  const OPS_URL = `http://localhost:${sys.ports.ops}`;

  // 2. Start VFS Node
  vfs = new VFS({
    id: 'mold-integration-node',
    storage: new MemoryStorage(),
  });
  mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
  await vfs.init();
  await mesh.start();

  // Wait for mesh handshake
  console.log("Waiting for geo-ops-node to connect...");
  const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
  await waitForMeshNodes(vfs, ['geo-ops-node']);

  // 3. Setup Jot Compiler and Parser
  const compiler = new JotCompiler(vfs);
  const parser = new JotParser();

  // 4. Fetch the operators catalog via Zenoh subscription
  console.log("Subscribing to sys/schema for catalog discovery...");
  let catalogReceived = null;
  vfs.events.on('notify', (selector, payload) => {
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
  for (const [name, schema] of Object.entries(catalog)) {
      compiler.registerOperator(name, { path: name, schema: schema });
  }

  await t.test('Evaluate parting line JOT expression', async () => {
    // Script uses Box and partLine to find the parting line
    const script = `
      part = Box(width=10, height=10, depth=10)
      part.partLine(dx=0.0, dy=0.0, dz=1.0, optimize=false) -> $out
    `.trim();

    const ast = parser.parse(script);
    const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
    
    assert.strictEqual(terminals.length, 1, 'Should output exactly one terminal');
    const resultSelector = terminals[0].selector;
    
    const response = await vfs.read(resultSelector);
    assert.ok(response, 'Should get response from VFS');
    
    const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
    assert.ok(result.geometry, 'Result should contain a geometry CID');
    
    const geoResp = await vfs.read(result.geometry);
    const geoText = new TextDecoder().decode(await consumeBytes(geoResp.stream));
    
    assert.ok(geoText.startsWith('V '), 'Geometry should start with V (vertices)');
    assert.ok(geoText.includes('S '), 'Geometry should contain S (segments)');
  });

  await t.test('Evaluate undercut analysis JOT expression', async () => {
    // Script uses Box and undercut to find safe, undercut, and flat faces
    const script = `
      part = Box(width=10, height=10, depth=10)
      part.undercut(dx=0.0, dy=0.0, dz=1.0, min_draft=0.5) -> $out
    `.trim();

    const ast = parser.parse(script);
    const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
    
    assert.strictEqual(terminals.length, 1, 'Should output exactly one terminal');
    const resultSelector = terminals[0].selector;
    
    const response = await vfs.read(resultSelector);
    assert.ok(response, 'Should get response from VFS');
    
    const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
    assert.ok(result.components && result.components.length === 3, 'Undercut output should be a Group with exactly 3 components');
    
    const components = result.components;
    const names = components.map(c => c.tags.name);
    assert.ok(names.includes('safe_faces'), 'Should have safe_faces component');
    assert.ok(names.includes('undercut_faces'), 'Should have undercut_faces component');
    assert.ok(names.includes('flat_faces'), 'Should have flat_faces component');
  });
});
