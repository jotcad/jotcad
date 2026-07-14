import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';

runIntegrationTest('jot/partLine - Parting line extraction and optimization', async ({ t, readData }) => {
  // Create a Box Subject (10x10x10)
  const box_sel = new Selector('jot/Box', { width: 10, height: 10, depth: 10 }).withOutput('$out');
  const box = await readData(box_sel);
  
  await t.test('should extract parting line along a manual vector', async () => {
    const part_sel = new Selector('jot/partLine', { 
      $in: box, 
      dx: 0.0, 
      dy: 0.0, 
      dz: 1.0 
    }).withOutput('$out');

    const result = await readData(part_sel);
    assert.ok(result.geometry, 'Should produce geometry');
    
    const geoText = await readData(result.geometry);
    assert.ok(geoText.startsWith('V '), 'Geometry should start with V (vertices)');
    assert.ok(geoText.includes('S '), 'Geometry should contain S (segments)');
  });

  await t.test('should optimize parting line vector automatically', async () => {
    const opt_sel = new Selector('jot/partLine', { 
      $in: box, 
      optimize: true 
    }).withOutput('$out');

    const result = await readData(opt_sel);
    assert.ok(result.geometry, 'Should produce geometry');
    
    // Check metadata tags
    assert.ok(result.tags, 'Result should have tags');
    assert.ok('dx' in result.tags, 'Result tags should contain dx');
    assert.ok('dy' in result.tags, 'Result tags should contain dy');
    assert.ok('dz' in result.tags, 'Result tags should contain dz');
    
    console.log(`[Integration Test] Auto-discovered optimal direction for Box: [${result.tags.dx}, ${result.tags.dy}, ${result.tags.dz}]`);
  });
});
