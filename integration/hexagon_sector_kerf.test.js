import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';

runIntegrationTest('Complex Mesh Expression: Hexagon Sector with Kerf', async ({ readData }) => {
  // THE EXPRESSION:
  // pdf(offset(loop(group(origin, nth(points(hexagon), 0), nth(points(hexagon), 1))), diameter=5))

  const hexagon = new Selector('jot/Hexagon/diameter', { diameter: 200 }).withOutput('$out');
  const points = new Selector('jot/points', { $in: hexagon }).withOutput('$out');
  const point0 = new Selector('jot/nth', { $in: points, index: [0] }).withOutput('$out');
  const point1 = new Selector('jot/nth', { $in: points, index: [1] }).withOutput('$out');
  const origin = new Selector('jot/nth', { 
    $in: new Selector('jot/points', { 
      $in: new Selector('jot/Box', { width: 1, height: 1, depth: 1 }).withOutput('$out') 
    }).withOutput('$out'), 
    index: [0] 
  }).withOutput('$out');
  const group = new Selector('jot/group', { $in: origin, shapes: [point0, point1] }).withOutput('$out');
  const loop = new Selector('jot/loop', { $in: group }).withOutput('$out');
  const fill = new Selector('jot/fill', { $in: loop }).withOutput('$out');
  const offset = new Selector('jot/offset', { $in: fill, diameter: 5.0 }).withOutput('$out');
  const pdf = new Selector('jot/pdf', { $in: offset, path: 'sector.pdf' }).withOutput('$out');

  console.log('[Test Sector] Requesting complex expression...');
  const pdfData = await readData(pdf);
  assert.ok(pdfData, 'Should return PDF data');
  
  // Check for PDF magic number
  const header = new TextDecoder().decode(pdfData.slice(0, 4));
  assert.strictEqual(header, '%PDF', 'Result should be a valid PDF buffer');

  console.log(`[Test Sector] SUCCESS: Generated ${pdfData.length} byte kerfed sector PDF.`);
});
