import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { decodeGeometry } from '../jot/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Conformal Wrapping (jot/conform)', async ({ t, vfs, readData }) => {
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

    const result = await readData(conform_sel);
    assert.ok(result, 'Should return a result');
    
    // Geometry check
    const geoText = await readData(result.geometry);
    const geo = decodeGeometry(geoText);

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

    const result = await readData(conform_sel);
    assert.ok(result, 'Should return a result');
    
    const geoText = await readData(result.geometry);
    const geo = decodeGeometry(geoText);

    let avgZ = 0;
    for (const v of geo.vertices) avgZ += v.z;
    avgZ /= geo.vertices.length;

    console.log(`[Test Conform] Closest Point Average Z: ${avgZ}`);
    // World Z=26, local Z = 26 - 30 = -4
    assert.ok(avgZ < -3 && avgZ > -5, `Shrink-wrap Z ${avgZ} should be near -4`);
  });
});
