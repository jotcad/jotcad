import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, MemoryStorage, Selector } from '../fs/src/index.js';
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

test('jot/partLine - Parting line extraction and optimization', { timeout: 60000 }, async (t) => {
  let sys;
  let vfs;
  let mesh;

  t.after(async () => {
    if (vfs) await vfs.close();
    if (sys) await sys.stop();
  });

  // 1. Launch the TEST system
  sys = await launchSystem('test/standard');
  const OPS_URL = `http://localhost:${sys.ports.ops}`;

  // 2. Start VFS Node
  vfs = new VFS({
    id: 'partline-integration-node',
    storage: new MemoryStorage(),
  });
  mesh = new MeshLink(vfs, [OPS_URL]);
  await vfs.init();
  await mesh.start();

  // Wait for mesh handshake
  await new Promise(r => setTimeout(r, 2000));

  // 3. Create a Box Subject (10x10x10)
  const box_sel = new Selector('jot/Box', { width: 10, height: 10, depth: 10 }).withOutput('$out');
  const boxResp = await vfs.read(box_sel);
  const box = JSON.parse(new TextDecoder().decode(await consumeBytes(boxResp.stream)));
  
  // 4. Test Parting Line along Z-up [0, 0, 1]
  await t.test('should extract parting line along a manual vector', async () => {
    const part_sel = new Selector('jot/partLine', { 
      $in: box, 
      dx: 0.0, 
      dy: 0.0, 
      dz: 1.0 
    }).withOutput('$out');

    const response = await vfs.read(part_sel);
    assert.ok(response, 'Should return a response');

    const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
    assert.ok(result.geometry, 'Should produce geometry');
    
    const geoResp = await vfs.read(result.geometry);
    const geoText = new TextDecoder().decode(await consumeBytes(geoResp.stream));
    
    assert.ok(geoText.startsWith('V '), 'Geometry should start with V (vertices)');
    assert.ok(geoText.includes('S '), 'Geometry should contain S (segments)');
  });

  // 5. Test Parting Line with optimize=true
  await t.test('should optimize parting line vector automatically', async () => {
    const opt_sel = new Selector('jot/partLine', { 
      $in: box, 
      optimize: true 
    }).withOutput('$out');

    const response = await vfs.read(opt_sel);
    assert.ok(response, 'Should return a response');

    const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
    assert.ok(result.geometry, 'Should produce geometry');
    
    // Check metadata tags
    assert.ok(result.tags, 'Result should have tags');
    assert.ok('dx' in result.tags, 'Result tags should contain dx');
    assert.ok('dy' in result.tags, 'Result tags should contain dy');
    assert.ok('dz' in result.tags, 'Result tags should contain dz');
    
    const dx_opt = result.tags.dx;
    const dy_opt = result.tags.dy;
    const dz_opt = result.tags.dz;
    console.log(`[Integration Test] Auto-discovered optimal direction for Box: [${dx_opt}, ${dy_opt}, ${dz_opt}]`);
  });
});
