import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('jot/decal - 3D wrapping on cube', async ({ vfs, readData }) => {
  // 1. Create a Cube Subject (1x1x1)
  const cube_sel = new Selector('jot/Box', { width: 1, height: 1, depth: 1 }).withOutput('$out');
  const cube = await readData(cube_sel);
  
  // 2. Create a large Plane Relief (4x4) at Z=2 looking down
  const relief_sel = new Selector('jot/Box', { width: 4, height: 4, depth: 0 }).withOutput('$out');
  const relief = await readData(relief_sel);
  
  const reliefShape = { 
    geometry: relief.geometry, 
    tf: "1 0 0 0 0 1 0 0 0 0 1 2" // At Z=2
  };

  // 3. Apply Decal
  const decal_sel = new Selector('jot/decal', { 
    $in: cube, 
    relief: reliefShape 
  }).withOutput('$out');

  const result = await readData(decal_sel);
  assert.ok(result.geometry, 'Should produce geometry');
  
  const geoText = await readData(result.geometry);
  
  assert.ok(geoText.startsWith('V '), 'Geometry should start with V tag');
  assert.ok(geoText.includes('T '), 'Geometry should contain T (triangles) tag');
});


