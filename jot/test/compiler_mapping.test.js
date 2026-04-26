import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCompiler Argument Mapping & Annotations', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Setup Mock Schemas
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [
        { name: 'width', type: 'jot:number', default: 10 },
        { name: 'height', type: 'jot:number', default: 10 },
        { name: 'depth', type: 'jot:number', default: 0 },
        { name: 'radius', type: 'jot:number' },
        { name: 'diameter', type: 'jot:number' }
      ]
    }
  });

  compiler.registerOperator('at', {
    path: 'jot/at',
    schema: {
      arguments: [
        { name: '$in', type: 'jot:shape' },
        { name: 'target', type: 'jot:shape' },
        { name: 'op', type: 'jot:operation' }
      ]
    }
  });

  compiler.registerOperator('corners', {
    path: 'jot/corners',
    schema: {
      arguments: [ { name: '$in', type: 'jot:shape' } ]
    }
  });

  compiler.registerOperator('cut', {
    path: 'jot/cut',
    schema: {
      arguments: [
        { name: '$in', type: 'jot:shape' },
        { name: 'tools', type: 'jot:shapes' }
      ]
    }
  });

  await t.test('Positional Mapping (Basic)', async () => {
    const ast = parser.parse('Box(20, 30)');
    const result = await compiler.evaluate(ast);
    assert.strictEqual(result.path, 'jot/Box');
    assert.strictEqual(result.parameters.width, 20);
    assert.strictEqual(result.parameters.height, 30);
    assert.strictEqual(result.parameters.depth, 0);
  });

  await t.test('Named Argument Mapping (Name=Value)', async () => {
    const ast = parser.parse('Box(width=50, depth=5)');
    const result = await compiler.evaluate(ast);
    assert.strictEqual(result.parameters.width, 50);
    assert.strictEqual(result.parameters.height, 10); // Default
    assert.strictEqual(result.parameters.depth, 5);
  });

  await t.test('Type Annotation Mapping (Type:Value)', async () => {
    // Explicitly type cut() as op: so it doesn't get stuffed into target
    const ast = parser.parse('Box(10).at(corners(), op:cut(Box(1)))');
    const result = await compiler.evaluate(ast);
    
    assert.strictEqual(result.path, 'jot/at');
    assert.strictEqual(result.parameters.target.path, 'jot/corners');
    assert.strictEqual(result.parameters.op.path, 'jot/cut');
  });

  await t.test('Combined Name and Type (Type:Name=Value)', async () => {
    const ast = parser.parse('Box(10).at(target=Box(2), op:action=cut(Box(1)))');
    const result = await compiler.evaluate(ast);
    
    assert.strictEqual(result.parameters.target.parameters.width, 2);
    // Note: C++ operator hydrate will map 'action' to whatever it needs if names don't match, 
    // but here we verify the compiler puts it in the 'action' slot because of nameHint.
    // Wait, the schema says 'op'. If I use 'action', it won't map unless the schema has it.
    // I'll test with the correct schema name.
    const ast2 = parser.parse('Box(10).at(target=Box(2), op:op=cut(Box(1)))');
    const result2 = await compiler.evaluate(ast2);
    assert.strictEqual(result2.parameters.op.path, 'jot/cut');
  });

  await t.test('Positional Yielding via Strict Type Matching', async () => {
    // Even without Name=, if an argument is marked op: it MUST skip a shape: slot
    const ast = parser.parse('Box(10).at(corners(), op:cut(Box(1)))');
    const result = await compiler.evaluate(ast);
    
    assert.strictEqual(result.parameters.target.path, 'jot/corners');
    assert.strictEqual(result.parameters.op.path, 'jot/cut');
  });

  await t.test('Normalization: radius to diameter', async () => {
    // Legacy support normalization
    const ast = parser.parse('Box(radius=10)');
    const result = await compiler.evaluate(ast);
    assert.strictEqual(result.parameters.diameter, 20);
    assert.strictEqual(result.parameters.radius, undefined);
  });

  await t.test('Lookahead Failure (Strict Mismatch)', async () => {
    // If I mark something as num: but the slot is jot:shape, it should not map
    const ast = parser.parse('Box(10).at(num:5, op:cut(Box(1)))');
    const result = await compiler.evaluate(ast);
    // target expects jot:shape. num:5 doesn't match. 
    // It should skip target and op (since num:5 doesn't match op either)
    assert.strictEqual(result.parameters.target, undefined);
  });
});
