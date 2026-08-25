import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('JotParser: Arrow Operator Definition AST', () => {
  const parser = new JotParser();
  const script = '(dome_d = 18.0, cap_d = 16.5) => Orb(dome_d).cut(Box(cap_d)) -> $out;';
  const ast = parser.parse(script);

  assert.strictEqual(ast.type, 'OPERATOR_DEF');
  assert.strictEqual(ast.params.length, 2);
  assert.strictEqual(ast.params[0].name, 'dome_d');
  assert.strictEqual(ast.params[0].default, 18.0);
  assert.strictEqual(ast.params[1].name, 'cap_d');
  assert.strictEqual(ast.params[1].default, 16.5);
  assert.strictEqual(ast.body.type, 'ASSIGNMENT');
  assert.strictEqual(ast.body.name, '$out');
});

test('JotCompiler: Arrow Operator Default Execution', async () => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Orb', {
    path: 'jot/Orb',
    schema: { arguments: [{ name: 'width', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });

  const script = '(dia = 25.0) => Orb(dia) -> $out;';
  const ast = parser.parse(script);

  const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
  assert.strictEqual(terminals.length, 1);
  const sel = terminals[0].selector;
  assert.strictEqual(sel.path, 'jot/Orb');
  assert.strictEqual(sel.parameters.width, 25.0);
});

test('JotCompiler: Arrow Operator Parameter Override', async () => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });

  const script = '(size = 10.0) => Box(size) -> $out;';
  const ast = parser.parse(script);

  // Override parameter
  const terminals = await compiler.evaluate(ast, { size: 42.0 }, { outputs: { $out: 'jot:shape' } });
  assert.strictEqual(terminals.length, 1);
  const sel = terminals[0].selector;
  assert.strictEqual(sel.path, 'jot/Box');
  assert.strictEqual(sel.parameters.size, 42.0);
});
