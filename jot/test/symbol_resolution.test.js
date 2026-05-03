import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

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
    assert.strictEqual(res.parameters.width, 100);
  });

  await t.test('preserves unprovided symbols with type verification', async () => {
    const ast = parser.parse('box(w)');
    const res = await compiler.evaluate(ast, {}, { w: 'jot:number' });
    assert.strictEqual(res.parameters.width.type, 'SYMBOL');
    assert.strictEqual(res.parameters.width.name, 'w');
  });

  await t.test('throws on typed symbol mismatch', async () => {
    const ast = parser.parse('box(s)');
    await assert.rejects(
      async () => await compiler.evaluate(ast, {}, { s: 'jot:string' }),
      /Compiler Error: Symbol 's' \(type jot:string\) used in jot:number slot 'width' of 'box'/
    );
  });
});
