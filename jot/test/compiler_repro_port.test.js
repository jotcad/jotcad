import test from 'node:test';
import assert from 'node:assert/strict';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';

test('JotCompiler Port Preservation Repro', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('pdf', {
    path: 'jot/pdf',
    schema: {
      arguments: [
        { name: '$in', type: 'jot:shape' },
        { name: 'path', type: 'jot:string' }
      ],
      outputs: { 
        "$out": { type: "file" }
      }
    }
  });

  const schema = {
    outputs: { 
      "$hex": { type: "file" } 
    }
  };

  await t.test('Should resolve PDF file output to schema output', async () => {
    // Mock a subject
    compiler.registerOperator('Hex', {
        path: 'jot/Hex',
        schema: { arguments: [], outputs: { "$out": { type: "jot:shape" } } }
    });

    // Expression: Hex().pdf("foo.pdf") -> $hex
    const ast = parser.parse('Hex().pdf("foo.pdf") -> $hex');
    const res = await compiler.evaluate(ast, {}, schema);
    
    assert.strictEqual(res.length, 1);
    const bundle = res[0];
    
    assert.strictEqual(bundle.port, '$hex', 'The bundle should indicate it fulfills the schema port $hex');
    assert.strictEqual(bundle.selector.path, 'jot/pdf');
    assert.strictEqual(bundle.selector.output, '$out', 'Output port should be "$out" by default');
  });
});
