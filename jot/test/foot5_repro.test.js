import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Reproduction: Foot5 Passthrough', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Mock Operators
  const ops = {
    'Hexagon': { arguments: [{ name: 'edgeToEdge', type: 'number' }], outputs: { $out: 'shape' } },
    'Box': { arguments: [{ name: 'w', type: 'number' }, { name: 'h', type: 'number' }], outputs: { $out: 'shape' } },
    'corners': { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } },
    'nth': { arguments: [{ name: '$in', type: 'shape' }, { name: 'index', type: 'number' }], outputs: { $out: 'shape' } },
    'o': { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } },
    'origin': { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } },
    'by': { arguments: [{ name: '$in', type: 'shape' }, { name: 'target', type: 'shape' }], outputs: { $out: 'shape' } }
  };

  for (const [name, schema] of Object.entries(ops)) {
    compiler.registerOperator(name, { path: `jot/${name}`, schema });
  }

  // 2. Mock VFS for User Op execution
  const mockVfs = {
    providers: new Map(),
    registerProvider(path, cb) { this.providers.set(path, cb); },
    async read(selector) {
      const provider = this.providers.get(selector.path);
      if (!provider) return selector;
      return await provider(this, selector);
    }
  };

  // 3. Register Foot5
  const foot5Script = `
    H = $in // Box(100, 100)
    C = H.corners().nth(0)
    H.by(C.o()) -> $out
  `;
  const foot5Schema = { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: { type: 'shape' } } };
  compiler.registerOperator('user/Foot5', { path: 'user/Foot5', script: foot5Script, schema: foot5Schema });

  mockVfs.registerProvider('user/Foot5', async (v, s) => {
    const innerCompiler = new JotCompiler(v);
    for (const [name, config] of compiler.operators.entries()) {
      innerCompiler.registerOperator(name, config[0]);
    }
    const innerAst = parser.parse(foot5Script);
    const results = await innerCompiler.evaluate(innerAst, { ...s.parameters }, foot5Schema);
    return results[0].selector;
  });

  // 4. Evaluate Test: Hexagon(edgeToEdge=246).Foot5()
  const testScript = 'Hexagon(edgeToEdge=246).Foot5() -> $out';
  const testAst = parser.parse(testScript);
  const terminals = await compiler.evaluate(testAst, {}, { outputs: { $out: 'shape' } });
  
  console.log('Outer Terminal Count:', terminals.length);
  assert.strictEqual(terminals.length, 1, 'Outer evaluation should produce exactly ONE terminal (Foot5)');
  const footSel = terminals[0].selector;
  
  console.log('Outer Terminal Selector:', JSON.stringify(footSel, null, 2));
  assert.strictEqual(footSel.path, 'user/Foot5');

  // 5. Execute Foot5
  const finalResult = await mockVfs.read(footSel);
  console.log('Final Executed Selector:', JSON.stringify(finalResult, null, 2));

  // Assertions
  assert.strictEqual(finalResult.path, 'jot/by', 'Should result in a "by" operation');
  assert.strictEqual(finalResult.parameters.$in.path, 'jot/Hexagon', 'Subject should be the Hexagon');
  assert.ok(finalResult.parameters.target.path === 'jot/o' || finalResult.parameters.target.path === 'jot/origin', 'Target should be the origin of the corner');
});
