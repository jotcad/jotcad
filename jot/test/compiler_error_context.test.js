import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCompiler Error Context: Should show problematic code in User Op error', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [{ name: 'size', type: 'jot:number' }],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  // A script that violates the "must be an assignment" rule
  // We use two calls to bypass the single-expression auto-assignment convenience.
  const script = 'Box(10); Box(20)'; 
  const ast = parser.parse(script);
  const schema = { outputs: { "$out": { type: "jot:shape" } } };

  try {
    await compiler.evaluate(ast, {}, schema, 'user/TestOp');
    assert.fail('Should have thrown a validation error');
  } catch (e) {
    assert.ok(e.message.includes('Box(10)'), `Error message should contain "Box(10)", got: "${e.message}"`);
    assert.ok(e.message.includes('Found CALL'), `Error message should mention the node type "CALL", got: "${e.message}"`);
  }
});

test('JotCompiler Error Context: Multi-line error context', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', { path: 'jot/Box', schema: { arguments: [], outputs: { $out: 'jot:shape' } } });
  compiler.registerOperator('Orb', { path: 'jot/Orb', schema: { arguments: [], outputs: { $out: 'jot:shape' } } });

  const script = 'Box() -> $out; Orb()'; 
  const ast = parser.parse(script);
  const schema = { outputs: { "$out": { type: "jot:shape" } } };

  try {
    await compiler.evaluate(ast, {}, schema, 'user/TestOp');
    assert.fail('Should have thrown a validation error');
  } catch (e) {
    assert.ok(e.message.includes('Orb()'), `Error message should identify the problematic "Orb()" call, got: "${e.message}"`);
  }
});
