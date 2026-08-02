import { runIntegrationTest } from './harness.js';
import { UGCEngine } from '../ugc/src/index.js';

runIntegrationTest('UGC Package UGCEngine Integration & Persisted Storage', async ({ compiler, parser, readData, vfs, mesh }) => {
  // 1. Instantiate the UGCEngine
  const ugc = new UGCEngine(vfs, mesh);
  await ugc.init(); // Load persisted registry

  const schema = {
    arguments: [
      { name: 'width', type: 'jot:number', default: 10 },
      { name: 'height', type: 'jot:number', default: 10 }
    ],
    outputs: { $out: { type: 'jot:shape' } }
  };

  // 2. Register user defined fulfiller using the package API
  const name = 'user/UgcPackageBox';
  const script = 'Box(width, height, 15) -> $out';
  
  console.log(`[Test] Registering fulfiller: ${name}`);
  await ugc.registerFulfiller({ name, schema, script, persist: true });

  // Verify it is listed in active catalog
  const activeList = ugc.listFulfillers();
  const found = activeList.find(op => op.path === name);
  if (!found) {
    throw new Error('Test Error: Registered operator not found in listFulfillers');
  }

  // 3. Evaluate a Jot expression calling our newly registered operator
  const evaluationScript = 'UgcPackageBox(25, 35) -> $out';
  console.log(`[Test] Evaluating expression calling UGC: "${evaluationScript}"`);

  const results = await ugc.evaluate(evaluationScript, {}, { outputs: { $out: { type: 'jot:shape' } } });
  const outTerm = results.find(t => t.port === '$out');
  if (!outTerm) {
    throw new Error('Test Error: Evaluator failed to return $out terminal');
  }

  // 4. Verify shape geometry resolves successfully
  const shapeData = await readData(outTerm.selector);
  console.log(`[Test] Successfully fulfilled shape geometry:`, JSON.stringify(shapeData).slice(0, 100) + '...');
  
  if (!shapeData) {
    throw new Error('Test Error: Shape geometry resolved to null');
  }
});
