import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('Compiler: User Op in Chain (A = Box.Red.rz)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Register Built-ins
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });
  compiler.registerOperator('Color', {
    path: 'jot/Color',
    schema: { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'c', type: 'jot:string' }], outputs: { $out: 'jot:shape' } }
  });
  compiler.registerOperator('rz', {
    path: 'jot/rz',
    schema: { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'a', type: 'jot:number' }], outputs: { $out: 'jot:shape' } }
  });

  // 2. Register User Op 'Red'
  const redSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };
  compiler.registerOperator('user/Red', {
    path: 'user/Red',
    schema: redSchema
  });

  // 3. Evaluate the chain
  const script = 'Box(15).Red().rz(0.25) -> $out';
  const ast = parser.parse(script);
  
  const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
  
  assert.strictEqual(terminals.length, 1);
  const sel = terminals[0].selector;

  // The final selector should be 'jot/rz'
  assert.strictEqual(sel.path, 'jot/rz');
  assert.strictEqual(sel.parameters.a, 0.25);
  
  // Its subject should be the 'user/Red' call
  const redSel = sel.parameters.$in;
  assert.strictEqual(redSel.path, 'user/Red');
  
  // Red's subject should be 'jot/Box'
  const boxSel = redSel.parameters.$in;
  assert.strictEqual(boxSel.path, 'jot/Box');
  assert.strictEqual(boxSel.parameters.size, 15);
});
