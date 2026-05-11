import { test } from 'node:test';
import assert from 'node:assert';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';
import { Selector } from '../../fs/src/vfs_core.js';
import { schemaToTypeMap } from './test_helpers.js';

test('DSL Declaration Integration (A = Font())', async (t) => {
  const compiler = new JotCompiler();
  const parser = new JotParser();

  // 1. Register operators
  compiler.registerOperator('Font', {
    path: 'jot/Font',
    schema: {
      arguments: [{ name: 'url', type: 'jot:string' }],
      outputs: { '$out': { type: 'jot:font' } }
    }
  });

  compiler.registerOperator('Text', {
    path: 'jot/text',
    schema: {
      arguments: [
        { name: 'text', type: 'jot:string' },
        { name: 'font', type: 'jot:font' },
        { name: 'size', type: 'jot:number', default: 10 }
      ],
      outputs: { '$out': { type: 'jot:shape' } }
    }
  });

  // 2. Multi-line script with declaration
  const script = `
    MyFont = Font("https://example.com/test.ttf");
    Text("Hello", font=MyFont, size=20) -> $out;
  `;

  // Documentation of the expected interface for this script
  const schema = {
      outputs: { "$out": { type: "jot:shape" } }
  };

  const ast = parser.parse(script);
  const res = await compiler.evaluate(ast, {}, schema);
  const results = res.map(r => r.selector);
  
  // 3. Verify
  assert.strictEqual(results.length, 1);
  const textSelector = results[0];
  assert.strictEqual(textSelector.path, 'jot/text');
  
  const fontArg = textSelector.parameters.font;
  assert.ok(fontArg instanceof Selector);
  assert.strictEqual(fontArg.path, 'jot/Font');
  assert.strictEqual(fontArg.parameters.url, 'https://example.com/test.ttf');
  
  console.log('✅ DSL Declaration verified: A = Font() correctly binds and resolves.');
});

test('Sequential Declaration Safety (No recursion)', async (t) => {
  const compiler = new JotCompiler();
  const parser = new JotParser();

  compiler.registerOperator('Text', {
    path: 'jot/text',
    schema: {
      arguments: [
        { name: 'text', type: 'jot:string' },
        { name: 'font', type: 'jot:font' }
      ],
      outputs: { '$out': { type: 'jot:shape' } }
    }
  });

  const schema = { outputs: { "$out": { type: "jot:shape" } } };

  // Test that A = Text(font=A) fails because A is not defined yet
  const script = 'A = Text("X", font=A);';
  const ast = parser.parse(script);
  
  try {
    await compiler.evaluate(ast, {}, schema);
    assert.fail('Should have thrown an error for unresolved symbol A');
  } catch (e) {
    assert.ok(e.message.includes("Missing required argument 'font'"), 'Should fail on missing font because A is unresolved');
    console.log('✅ Declaration safety verified: Recursive declaration is prevented.');
  }
});
