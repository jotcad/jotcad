import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';

runIntegrationTest('Mold Generation & Analysis - JOT Expression Integration', async ({ t, readData, evaluate }) => {
  await t.test('Evaluate parting line JOT expression', async () => {
    const script = `
      part = Box(width=10, height=10, depth=10)
      part.partLine(dx=0.0, dy=0.0, dz=1.0, optimize=false) -> $out
    `.trim();

    const terminals = await evaluate(script);
    assert.strictEqual(terminals.length, 1, 'Should output exactly one terminal');
    
    const result = await readData(terminals[0].selector);
    assert.ok(result.geometry, 'Result should contain a geometry CID');
    
    const geoText = await readData(result.geometry);
    assert.ok(geoText.startsWith('V '), 'Geometry should start with V (vertices)');
    assert.ok(geoText.includes('S '), 'Geometry should contain S (segments)');
  });

  await t.test('Evaluate undercut analysis JOT expression', async () => {
    const script = `
      part = Box(width=10, height=10, depth=10)
      part.undercut(dx=0.0, dy=0.0, dz=1.0, min_draft=0.5) -> $out
    `.trim();

    const terminals = await evaluate(script);
    assert.strictEqual(terminals.length, 1, 'Should output exactly one terminal');
    
    const result = await readData(terminals[0].selector);
    assert.ok(result.components && result.components.length === 3, 'Undercut output should be a Group with exactly 3 components');
    
    const names = result.components.map(c => c.tags.name);
    assert.ok(names.includes('safe_faces'), 'Should have safe_faces component');
    assert.ok(names.includes('undercut_faces'), 'Should have undercut_faces component');
    assert.ok(names.includes('flat_faces'), 'Should have flat_faces component');
  });
});
