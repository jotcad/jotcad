import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, MemoryStorage, Selector } from '../fs/src/index.js';
import { launchSystem } from '../orchestrator.js';
import { waitForMeshNodes } from './vfs_test_helpers.js';

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

test('jot/decal - 3D wrapping on cube', { timeout: 60000 }, async (t) => {
  let sys;
  let vfs;
  let mesh;

  try {
    // 1. Launch the TEST system
    sys = await launchSystem('test/standard');

    // 2. Start VFS Node
    vfs = new VFS({
      id: 'decal-integration-node',
      storage: new MemoryStorage(),
    });
    mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    await vfs.init();
    await mesh.start();

    // Wait for mesh catalog sync
    await waitForMeshNodes(vfs, ['geo-primitives-node', 'geo-mapping-node']);

    // 1. Create a Cube Subject (1x1x1)
    const cube_sel = new Selector('jot/Box', { width: 1, height: 1, depth: 1 }).withOutput('$out');
    const cubeResp = await vfs.read(cube_sel);
    assert.ok(cubeResp, 'Should return cube response');
    const cube = JSON.parse(new TextDecoder().decode(await consumeBytes(cubeResp.stream)));
    
    // 2. Create a large Plane Relief (4x4) at Z=2 looking down
    const relief_sel = new Selector('jot/Box', { width: 4, height: 4, depth: 0 }).withOutput('$out');
    const reliefResp = await vfs.read(relief_sel);
    assert.ok(reliefResp, 'Should return relief response');
    const relief = JSON.parse(new TextDecoder().decode(await consumeBytes(reliefResp.stream)));
    
    const reliefShape = { 
      geometry: relief.geometry, 
      tf: "1 0 0 0 0 1 0 0 0 0 1 2" // At Z=2
    };

    // 3. Apply Decal
    const decal_sel = new Selector('jot/decal', { 
      $in: cube, 
      relief: reliefShape 
    }).withOutput('$out');

    const response = await vfs.read(decal_sel);
    assert.ok(response, 'Should return a response');

    const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
    assert.ok(result.geometry, 'Should produce geometry');
    
    const geoResp = await vfs.read(result.geometry);
    const geoText = new TextDecoder().decode(await consumeBytes(geoResp.stream));
    
    assert.ok(geoText.startsWith('V '), 'Geometry should start with V tag');
    assert.ok(geoText.includes('T '), 'Geometry should contain T (triangles) tag');
  } finally {
    console.log("[Test] Running cleanup...");
    if (mesh) await mesh.stop();
    if (vfs) await vfs.close();
    if (sys) await sys.stop();
  }
});
