import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Compiler: Custom Operator Subject Injection ($in)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Mock the geometry operators
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [
        { name: 'width', type: 'number' },
        { name: 'height', type: 'number' }
      ],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('Disk', {
    path: 'jot/Disk',
    schema: {
      arguments: [{ name: 'radius', type: 'number' }],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('corners', {
    path: 'jot/corners',
    schema: {
      arguments: [{ name: '$in', type: 'shape' }],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('nth', {
    path: 'jot/nth',
    schema: {
      arguments: [
        { name: '$in', type: 'shape' },
        { name: 'index', type: 'number' }
      ],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('o', {
    path: 'jot/origin',
    schema: {
      arguments: [{ name: '$in', type: 'shape' }],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('by', {
    path: 'jot/by',
    schema: {
      arguments: [
        { name: '$in', type: 'shape' },
        { name: 'target', type: 'shape' }
      ],
      outputs: { $out: { type: 'shape' } }
    }
  });

  compiler.registerOperator('clip', {
    path: 'jot/clip',
    schema: {
      arguments: [
        { name: '$in', type: 'shape' },
        { name: 'tool', type: 'shape' }
      ],
      outputs: { $out: { type: 'shape' } }
    }
  });

  // 2. Register the custom 'Foot' operator
  // Script: $in.by(corners().nth(0).o()).clip(Disk(40)) -> $out
  compiler.registerOperator('Foot', {
    script: '$in.by(corners().nth(0).o()).clip(Disk(40)) -> $out',
    schema: {
      arguments: [{ name: '$in', type: 'shape', affiliate: '$out' }],
      outputs: { $out: { type: 'shape' } }
    }
  });

  // 3. Test 1: Box(100, 100).Foot()
  await t.test('Should inject subject into Foot method call', async () => {
    const script = 'Box(100, 100).Foot() -> $out';
    const ast = parser.parse(script);
    
    // We expect the compiler to produce a Selector that will be expanded by the mesh provider,
    // OR we want the compiler to expand it itself.
    // For now, let's see what it produces.
    const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'shape' } });
    const footSel = terminals[0].selector;
    
    console.log('Foot Selector:', JSON.stringify(footSel, null, 2));
    assert.strictEqual(footSel.path, 'Foot');
    assert.ok(footSel.parameters.$in, 'Subject $in should be present in Foot parameters');
    assert.strictEqual(footSel.parameters.$in.path, 'jot/Box');
  });

  // 4. Test 2: Foot() - Should fail with missing $in
  await t.test('Should fail if Foot is called without a subject', async () => {
    const script = 'Foot() -> $out';
    const ast = parser.parse(script);
    
    try {
      await compiler.evaluate(ast, {}, { outputs: { $out: 'shape' } });
      assert.fail('Should have thrown missing $in error');
    } catch (e) {
      assert.match(e.message, /Missing required argument '\$in'/);
      console.log('Caught Expected Error:', e.message);
    }
  });
});
