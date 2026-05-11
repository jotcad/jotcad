import { test } from 'node:test';
import assert from 'node:assert';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('Schema-Driven: Arrow Assignment (A -> B)', async (t) => {
  const compiler = new JotCompiler();
  const parser = new JotParser();

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [{ name: 'size', type: 'jot:number' }],
      outputs: { '$out': { type: 'jot:shape' } }
    }
  });

  const script = 'Box(10) -> $out';
  const ast = parser.parse(script);
  
  // Test with explicit schema extraction
  const schema = {
      outputs: { '$out': { type: 'jot:shape' } }
  };
  
  const results = await compiler.evaluate(ast, {}, schema);
  
  assert.strictEqual(results.length, 1);
  assert.strictEqual(results[0].selector.output, '$out');
  assert.strictEqual(results[0].selector.path, 'jot/Box');
  assert.strictEqual(results[0].selector.parameters.size, 10);
});

test('Schema-Driven: Multi-Port Assignment', async (t) => {
    const compiler = new JotCompiler();
    const parser = new JotParser();
  
    compiler.registerOperator('Box', {
      path: 'jot/Box',
      schema: { arguments: [{ name: 's', type: 'jot:number' }], outputs: { '$out': { type: 'jot:shape' } } }
    });
    compiler.registerOperator('Orb', {
        path: 'jot/Orb',
        schema: { arguments: [{ name: 'r', type: 'jot:number' }], outputs: { '$out': { type: 'jot:shape' } } }
      });
  
    const script = `
        Box(10) -> $main;
        Orb(5) -> $core;
    `;
    
    const schema = {
        outputs: { 
            '$main': { type: 'jot:shape' },
            '$core': { type: 'jot:shape' }
        }
    };
    
    const ast = parser.parse(script);
    const results = await compiler.evaluate(ast, {}, schema);
    
    assert.strictEqual(results.length, 2);
    const main = results.find(r => r.selector.output === '$main').selector;
    const core = results.find(r => r.selector.output === '$core').selector;
    
    assert.strictEqual(main.path, 'jot/Box');
    assert.strictEqual(core.path, 'jot/Orb');
});

test('Schema-Driven: $in Binding and Chaining', async (t) => {
    const compiler = new JotCompiler();
    const parser = new JotParser();
  
    compiler.registerOperator('Box', {
      path: 'jot/Box',
      schema: { arguments: [{ name: 's', type: 'jot:number' }], outputs: { '$out': { type: 'jot:shape' } } }
    });
    
    compiler.registerOperator('cut', {
        path: 'jot/cut',
        schema: { 
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'tool', type: 'jot:shape' }
            ],
            outputs: { '$out': { type: 'jot:shape' } } 
        }
    });

    // Simulate VFS/Mesh injection of $in
    const mockIn = new Selector('jot/Box', { s: 50 }).withOutput('$out');
    const script = '$in.cut(Box(10)) -> $out';
    
    const schema = {
        outputs: { '$out': { type: 'jot:shape' } }
    };

    const ast = parser.parse(script);
    const results = await compiler.evaluate(ast, { '$in': mockIn }, schema);
    
    assert.strictEqual(results.length, 1);
    assert.strictEqual(results[0].selector.path, 'jot/cut');
    assert.strictEqual(results[0].selector.parameters.$in.path, 'jot/Box');
    assert.strictEqual(results[0].selector.parameters.$in.parameters.s, 50);
    assert.strictEqual(results[0].selector.parameters.tool.path, 'jot/Box');
    assert.strictEqual(results[0].selector.parameters.tool.parameters.s, 10);
});
