import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('Compiler: Nested User Op Propagation', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Mock built-ins
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'size', type: 'number' }], outputs: { $out: { type: 'shape' } } }
  });
  compiler.registerOperator('Color', {
    path: 'jot/Color',
    schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'c', type: 'string' }], outputs: { $out: { type: 'shape' } } }
  });

  // 2. Mock VFS with nested execution logic
  const mockVfs = {
    providers: new Map(),
    registerProvider(path, cb) { this.providers.set(path, cb); },
    async read(selector) {
      const provider = this.providers.get(selector.path);
      if (!provider) throw new Error(`No provider for ${selector.path}`);
      return await provider(this, selector);
    }
  };

  const createProvider = (path, script, schema) => {
    mockVfs.registerProvider(path, async (v, s) => {
      const innerCompiler = new JotCompiler(v);
      
      // Register all operators (built-ins + other user ops)
      innerCompiler.registerOperator('Box', compiler.operators.get('Box')[0]);
      innerCompiler.registerOperator('Color', compiler.operators.get('Color')[0]);
      
      // Register current user ops from the registry
      for (const [p, config] of compiler.operators.entries()) {
          if (p.startsWith('user/')) {
              innerCompiler.registerOperator(p, config[0]);
          }
      }

      const innerParser = new JotParser();
      const ast = innerParser.parse(script);
      const params = { ...s.parameters };
      
      // Simulate JotRegistry result extraction
      const results = await innerCompiler.evaluate(ast, params, schema);
      return results[0].selector;
    });
  };

  // 3. Define the Chain: Box -> Step1 (Red) -> Step2 (Blue)
  const step1Schema = { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: { type: 'shape' } } };
  const step2Schema = { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: { type: 'shape' } } };

  compiler.registerOperator('user/Step1', { path: 'user/Step1', script: '$in.Color("red") -> $out', schema: step1Schema });
  compiler.registerOperator('user/Step2', { path: 'user/Step2', script: '$in.Step1().Color("blue") -> $out', schema: step2Schema });

  createProvider('user/Step1', '$in.Color("red") -> $out', step1Schema);
  createProvider('user/Step2', '$in.Step1().Color("blue") -> $out', step2Schema);

  // 4. Evaluate: Box(100).Step2()
  const script = 'Box(100).Step2() -> $out';
  const ast = parser.parse(script);
  
  const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'shape' } });
  assert.strictEqual(terminals.length, 1, 'Should produce exactly one terminal');
  const step2Sel = terminals[0].selector;

  assert.strictEqual(step2Sel.path, 'user/Step2', 'Outer call should produce Step2 selector');
  assert.strictEqual(step2Sel.parameters.$in.path, 'jot/Box', 'Step2 should have Box as input');

  // 5. Execute Step2 (which calls Step1)
  const step2Result = await mockVfs.read(step2Sel);
  console.log('Step 2 Result (Expected Color blue of Step 1):', JSON.stringify(step2Result, null, 2));

  // The result of Step2 should be a Color(blue) operation...
  assert.strictEqual(step2Result.path, 'jot/Color', 'Result should be the outer color op');
  assert.strictEqual(step2Result.parameters.c, 'blue');
  
  // ...whose subject is the result of Step1 (Color red)
  const step1Result = step2Result.parameters.$in;
  assert.strictEqual(step1Result.path, 'user/Step1', 'Step2 subject should be Step1 call');
  
  // Now execute the inner Step1
  const finalResult = await mockVfs.read(step1Result);
  console.log('Final Result (Expected Color red of Box):', JSON.stringify(finalResult, null, 2));

  assert.strictEqual(finalResult.path, 'jot/Color', 'Final result should be the red color op');
  assert.strictEqual(finalResult.parameters.c, 'red');
  assert.strictEqual(finalResult.parameters.$in.path, 'jot/Box', 'Step1 subject should be the original Box');
});

test('Compiler: User Op Versioning (:vN)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', { path: 'jot/Box', schema: { arguments: [{ name: 'size', type: 'number' }], outputs: { $out: 'shape' } } });
  compiler.registerOperator('Color', { path: 'jot/Color', schema: { arguments: [{ name: '$in', type: 'shape' }, { name: 'c', type: 'string' }], outputs: { $out: 'shape' } } });

  // 1. Register v1
  compiler.registerOperator('user/MyOp:v1', {
    path: 'user/MyOp:v1',
    schema: { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } }
  });

  const script = 'Box(10).MyOp() -> $out';
  const ast = parser.parse(script);
  const terminals1 = await compiler.evaluate(ast, {}, { outputs: { $out: 'shape' } });
  assert.strictEqual(terminals1[0].selector.path, 'user/MyOp:v1', 'Should target v1');

  // 2. Register v2 (Replacing v1)
  compiler.registerOperator('user/MyOp:v2', {
    path: 'user/MyOp:v2',
    schema: { arguments: [{ name: '$in', type: 'shape' }], outputs: { $out: 'shape' } }
  });

  const terminals2 = await compiler.evaluate(ast, {}, { outputs: { $out: 'shape' } });
  assert.strictEqual(terminals2[0].selector.path, 'user/MyOp:v2', 'Should target v2 after replacement');
  assert.strictEqual(compiler.operators.get('MyOp').length, 1, 'Should only have one version registered for the short name');
});
