import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, DiskStorage, Selector } from '../fs/src/index.js';
import { decodeGeometry } from '../jot/src/index.js';
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

test('Conformal Wrapping (jot/conform)', { timeout: 60000 }, async (t) => {
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
    id: 'conform-test-node',
    storage: new DiskStorage('.vfs_storage_conform_test'),
  });
  mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
  await vfs.init();
  await mesh.start();

  // Wait for mesh handshake
  await new Promise(r => setTimeout(r, 2000));

  await t.test('should project a disk onto an orb with offset', async () => {
    const target_sel = new Selector('jot/Orb', { diameter: 50, zag: 0.1 }).withOutput('$out');
    const subject_sel = new Selector('jot/Disk', { diameter: 20, zag: 0.2 }).withOutput('$out');
    const moved_subject = new Selector('jot/mz', { $in: subject_sel, z: 30 }).withOutput('$out');
    const conform_sel = new Selector('jot/conform', {
        $in: moved_subject,
        target: target_sel,
        direction: [0, 0, -1],
        offset: 1.0
    }).withOutput('$out');

    const response = await vfs.read(conform_sel);
    assert.ok(response, 'Should return a response');

    const resultBytes = await consumeBytes(response.stream);
    const result = JSON.parse(new TextDecoder().decode(resultBytes));
    
    // Geometry check
    assert.ok(result.geometry, 'Result should have geometry');
    const geoResponse = await vfs.read(result.geometry);
    const geoBytes = await consumeBytes(geoResponse.stream);
    const geo = decodeGeometry(new TextDecoder().decode(geoBytes));

    assert.ok(geo.vertices.length > 0, 'Should have vertices');
    
    let avgZ = 0;
    for (const v of geo.vertices) {
        avgZ += v.z;
    }
    avgZ /= geo.vertices.length;

    console.log(`[Test Conform] Average Z: ${avgZ}`);
    
    // Orb R=25. Disk at Z=30 projected -Z. 
    // Center world Z = 25 + 1 = 26.
    // In local space (subject was at Z=30), world Z=26 is local Z=-4.
    // So avgZ should be roughly in the -4 to -7 range.
    assert.ok(avgZ < -3 && avgZ > -8, `Average Z ${avgZ} should be in wrap range (-8 to -3)`);
  });

  await t.test('should perform shrink-wrap (closest point) if direction is zero', async () => {
    // Subject inside the sphere, offset -5 (should move to Z=20 world, local Z=-10)
    const target_sel = new Selector('jot/Orb', { diameter: 50, zag: 0.1 }).withOutput('$out');
    const subject_sel = new Selector('jot/Disk', { diameter: 5, zag: 0.5 }).withOutput('$out');
    const moved_subject = new Selector('jot/mz', { $in: subject_sel, z: 30 }).withOutput('$out');
    
    const conform_sel = new Selector('jot/conform', {
        $in: moved_subject,
        target: target_sel,
        direction: [0, 0, 0], // Closest point
        offset: 1.0
    }).withOutput('$out');

    const response = await vfs.read(conform_sel);
    const resultBytes = await consumeBytes(response.stream);
    const result = JSON.parse(new TextDecoder().decode(resultBytes));
    
    const geoResponse = await vfs.read(result.geometry);
    const geoBytes = await consumeBytes(geoResponse.stream);
    const geo = decodeGeometry(new TextDecoder().decode(geoBytes));

    let avgZ = 0;
    for (const v of geo.vertices) avgZ += v.z;
    avgZ /= geo.vertices.length;

    console.log(`[Test Conform] Closest Point Average Z: ${avgZ}`);
    // World Z=26, local Z = 26 - 30 = -4
    assert.ok(avgZ < -3 && avgZ > -5, `Shrink-wrap Z ${avgZ} should be near -4`);
  });
});
