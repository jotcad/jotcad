import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('JotParser: Static Constant Folding on Literals', () => {
  const parser = new JotParser();

  // Pure literals are eagerly folded at parse time
  const ast1 = parser.parse('1 + 2 * 3 -> $out;');
  assert.strictEqual(ast1.type, 'ASSIGNMENT');
  assert.strictEqual(ast1.value, 7);

  const ast2 = parser.parse('(1 + 2) * 3 -> $out;');
  assert.strictEqual(ast2.value, 9);

  const ast3 = parser.parse('10 - 4 / 2 -> $out;');
  assert.strictEqual(ast3.value, 8);
});

test('JotParser: Whitespace-Sensitive Infix Desugaring', () => {
  const parser = new JotParser();

  // Infix with symbols desugars into math/* calls
  const ast = parser.parse('16.5 + clearance * 2 -> $out;');
  assert.strictEqual(ast.type, 'ASSIGNMENT');
  assert.strictEqual(ast.value.type, 'CALL');
  assert.strictEqual(ast.value.name, 'math/add');
  assert.strictEqual(ast.value.args[0], 16.5);
  
  const multArg = ast.value.args[1];
  assert.strictEqual(multArg.type, 'CALL');
  assert.strictEqual(multArg.name, 'math/multiply');
  assert.deepStrictEqual(multArg.args, [{ type: 'SYMBOL', name: 'clearance' }, 2]);
});

test('JotParser: Fractions and Negatives Without Spaces', () => {
  const parser = new JotParser();

  // 1/12 literal fraction without spaces
  const ast1 = parser.parse('Box(turns=1/12) -> $out;');
  assert.strictEqual(ast1.value.type, 'CALL');
  assert.strictEqual(ast1.value.args[0].nameHint, 'turns');
  assert.strictEqual(ast1.value.args[0].value, 1/12);

  // Negative number literal without spaces
  const ast2 = parser.parse('Box(z=-2.5) -> $out;');
  assert.strictEqual(ast2.value.args[0].value, -2.5);
});

test('JotCompiler: Dynamic Constant Folding with Symbols', async () => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });

  const script = 'clearance = 0.4; Box(size = 16.5 + clearance * 2) -> $out;';
  const ast = parser.parse(script);

  const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
  assert.strictEqual(terminals.length, 1);
  const sel = terminals[0].selector;
  assert.strictEqual(sel.path, 'jot/Box');
  assert.strictEqual(sel.parameters.size, 17.3);
});

test('JotCompiler: Functional math/* Calls', async () => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });

  const script = 'Box(size = math/add(10, math/multiply(3, 4))) -> $out;';
  const ast = parser.parse(script);

  const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
  assert.strictEqual(terminals.length, 1);
  assert.strictEqual(terminals[0].selector.parameters.size, 22);
});
