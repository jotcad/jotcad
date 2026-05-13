import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Repro: Box(20, 20) -> $out in User Op', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [
        { name: 'width', type: 'number' },
        { name: 'height', type: 'number' }
      ],
      outputs: { "$out": { type: "shape" } }
    }
  });

  const script = 'Box(20, 20) -> $out';
  const ast = parser.parse(script);
  
  console.log('AST:', JSON.stringify(ast, null, 2));

  const schema = {
    outputs: { "$out": { type: "shape" } }
  };

  // This should NOT throw if the bug is not in the compiler's evaluate method itself
  const res = await compiler.evaluate(ast, {}, schema);
  assert.ok(res);
  assert.strictEqual(res[0].selector.path, 'jot/Box');
  assert.strictEqual(res[0].selector.parameters.width, 20);
  assert.strictEqual(res[0].selector.parameters.height, 20);
});
