import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('User Operator Interaction Integration', async ({ vfs, compiler, parser, readData }) => {
  const footScript = `
H = $in
C = H.corners().nth(0)
H .by(C.o())
  .clip(Disk(40))
  .ez(10)
  .cut(mx(4).mz(-4))
  .cut(Disk(5.1).ez(50).rx(1/4).mx(10).rz(1/6, -1/6))
  .cut(Disk(1.4).mx(10).ez([0, 20]))
  .ry(1/2) -> $out
  `;
  const footSchema = {
    path: 'user/Foot',
    inputs: { '$in': { type: 'jot:shape' } },
    arguments: [],
    outputs: { '$out': { type: 'jot:shape' } }
  };

  const testScript = `
Hexagon(edgeToEdge=248).Foot().pdf('hex_foot2.pdf') -> $hex
  `;
  const testSchema = {
    path: 'user/Test',
    arguments: [],
    outputs: { '$hex': { type: 'file', mimeType: 'application/pdf' } }
  };

  compiler.registerOperator('user/Foot', { path: 'user/Foot', script: footScript, schema: footSchema });
  compiler.registerOperator('user/Test', { path: 'user/Test', script: testScript, schema: testSchema });

  // 4. VFS Providers
  vfs.registerProvider('user/Foot', async (vfs, selector) => {
    const ast = parser.parse(footScript);
    const results = await compiler.evaluate(ast, selector.parameters, footSchema, 'user/Foot');
    if (!results || results.length === 0) throw new Error('user/Foot failed to produce output');
    return await vfs.read(results[0].selector);
  });

  vfs.registerProvider('user/Test', async (vfs, selector) => {
    const ast = parser.parse(testScript);
    const results = await compiler.evaluate(ast, selector.parameters, testSchema, 'user/Test');
    const hexResult = results.find(r => r.port === '$hex');
    if (!hexResult) throw new Error('user/Test failed to produce $hex output');
    return await vfs.read(hexResult.selector);
  });

  // 5. Execution
  console.log('[Test] Requesting user/Test output...');
  try {
    const selector = new Selector('user/Test').withOutput('$hex');
    const result = await vfs.readSelector(selector);
    assert.strictEqual(result.metadata.state, 'AVAILABLE');

    const pdfData = await readData(selector);
    assert.ok(pdfData.length > 0, 'PDF should not be empty');
    assert.strictEqual(new TextDecoder().decode(pdfData.slice(0, 5)), '%PDF-', 'Valid PDF header');
  } catch (err) {
    console.error('[Test] FAIL:', err);
    throw err;
  }
});
