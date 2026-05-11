import { test } from 'node:test';
import assert from 'node:assert';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('Typography DSL Integration (Exact Type Match)', async (t) => {
  const compiler = new JotCompiler();

  // 1. Register the operators with their formal types
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

  // 2. Compile a DSL expression that nests them
  const code = 'Text("Hole Test", font=Font("https://example.com/font.ttf"), size=25)';
  const ast = {
    type: 'ASSIGNMENT',
    name: '$out',
    value: {
      type: 'CALL',
      name: 'Text',
      args: [
        { type: 'ANNOTATED_ARG', nameHint: 'text', value: "Hole Test" },
        { type: 'ANNOTATED_ARG', nameHint: 'font', value: {
            type: 'CALL',
            name: 'Font',
            args: [{ type: 'ANNOTATED_ARG', nameHint: 'url', value: "https://example.com/font.ttf" }]
        }},
        { type: 'ANNOTATED_ARG', nameHint: 'size', value: 25 }
      ]
    }
  };

  const schema = { outputs: { "$out": { type: "jot:shape" } } };
  const results = await compiler.evaluate(ast, {}, schema);
  
  // 3. Verify the resulting selector structure
  assert.strictEqual(results.length, 1);
  const textSelector = results[0];
  assert.strictEqual(textSelector.path, 'jot/text');
  assert.strictEqual(textSelector.parameters.text, 'Hole Test');
  assert.strictEqual(textSelector.parameters.size, 25);
  
  // The 'font' parameter should be the Selector for the Font operation
  const fontSelector = textSelector.parameters.font;
  assert.ok(fontSelector instanceof Selector);
  assert.strictEqual(fontSelector.path, 'jot/Font');
  assert.strictEqual(fontSelector.parameters.url, 'https://example.com/font.ttf');
  
  console.log('✅ Typography DSL verified: Font() nested in Text() matches via exact typing.');
});
