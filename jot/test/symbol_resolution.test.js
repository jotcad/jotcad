import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { schemaToTypeMap } from './test_helpers.js';

test('JotCAD Evaluator: Symbol Resolution', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('box', {
    path: 'shape/box',
    schema: {
      arguments: [{ name: 'width', type: 'jot:number' }],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  await t.test('resolves provided symbols', async () => {
    const ast = parser.parse('box(w)');
    const res = await compiler.evaluate(ast, { w: 100 });
    const resolved = res[0];
    assert.strictEqual(resolved.parameters.width, 100);
  });

  await t.test('preserves unprovided symbols with type verification', async () => {
    const ast = parser.parse('box(w)');
    
    // Schema definition for the late-bound symbol 'w'
    const schema = {
        arguments: [{ name: 'w', type: 'jot:number' }],
        outputs: { '$out': { type: 'jot:shape' } }
    };
    const originalTypeMap = { w: 'jot:number', '$out': 'jot:shape' };
    
    // 1. Assert Schema-to-TypeMap equivalence
    assert.deepStrictEqual(schemaToTypeMap(schema), originalTypeMap);

    // 2. Pass original map to preserve behavior
    const res = await compiler.evaluate(ast, {}, originalTypeMap);
    const resolved = res[0];
    assert.strictEqual(resolved.parameters.width.type, 'jot:number');
    assert.strictEqual(resolved.parameters.width.name, 'w');
  });

  await t.test('throws on typed symbol mismatch', async () => {
    const ast = parser.parse('box(s)');
    
    const schema = {
        arguments: [{ name: 's', type: 'jot:string' }],
        outputs: { '$out': { type: 'jot:shape' } }
    };
    const originalTypeMap = { s: 'jot:string', '$out': 'jot:shape' };

    // 1. Assert Schema-to-TypeMap equivalence
    assert.deepStrictEqual(schemaToTypeMap(schema), originalTypeMap);

    await assert.rejects(
      async () => await compiler.evaluate(ast, {}, originalTypeMap),
      /Missing required argument 'width'/
    );
  });
});
