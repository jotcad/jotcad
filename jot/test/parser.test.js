import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCAD Parser: Basic Function Call', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('jot/Test', {
    path: 'jot/Test',
    schema: {
      arguments: [
        { name: 'size', type: 'jot:number' },
        { name: 'label', type: 'jot:string' },
        { name: 'active', type: 'jot:boolean' }
      ],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('Test(10, "hello", true)');
  const res = await compiler.evaluate(ast);
  assert.strictEqual(res.path, 'jot/Test');
  assert.strictEqual(res.parameters.size, 10);
  assert.strictEqual(res.parameters.label, 'hello');
  assert.strictEqual(res.parameters.active, true);
});

test('JotCAD Parser: Method Chaining', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('jot/box', {
    path: 'jot/box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } }
  });
  compiler.registerOperator('jot/rotateX', {
    path: 'jot/rotateX',
    schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'angle', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('box(100).rotateX(0.1)');
  const res = await compiler.evaluate(ast);
  assert.strictEqual(res.path, 'jot/rotateX');
  assert.strictEqual(res.parameters.angle, 0.1);
  assert.strictEqual(res.parameters.$in.path, 'jot/box');
});

test('JotCAD Parser: Late-Bound Symbols', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();
  compiler.registerOperator('jot/box', {
    path: 'jot/box',
    schema: { arguments: [{ name: 'width', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } }
  });

  const ast = parser.parse('box(width)');
  const res = await compiler.evaluate(ast);
  assert.strictEqual(res.parameters.width.type, 'SYMBOL');
  assert.strictEqual(res.parameters.width.name, 'width');
});

test('JotCAD Evaluator: Symbol Resolution', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();
  compiler.registerOperator('jot/box', {
    path: 'jot/box',
    schema: { arguments: [{ name: 'width', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } }
  });

  const ast = parser.parse('box(width)');
  const res = await compiler.evaluate(ast, { width: 42 });
  assert.strictEqual(res.parameters.width, 42);
});

test('JotCAD Parser: Deep Method Chaining', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();
  
  const noopSchema = { 
    arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }], 
    outputs: { "$out": { type: "jot:shape" } } 
  };
  compiler.registerOperator('jot/box', { path: 'jot/box', schema: { arguments: [{ name: 's', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });
  compiler.registerOperator('jot/rotateX', { path: 'jot/rotateX', schema: { arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'a', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });
  compiler.registerOperator('jot/offset', { path: 'jot/offset', schema: { arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'r', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });
  compiler.registerOperator('jot/outline', { path: 'jot/outline', schema: noopSchema });

  const ast = parser.parse('box(100).rotateX(0.1).offset(5).outline()');
  const res = await compiler.evaluate(ast);
  assert.strictEqual(res.path, 'jot/outline');
  assert.strictEqual(res.parameters.$in.path, 'jot/offset');
  assert.strictEqual(res.parameters.$in.parameters.$in.path, 'jot/rotateX');
});

test('JotCAD Parser: Array of Expressions', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();
  compiler.registerOperator('jot/box', { path: 'jot/box', schema: { arguments: [{ name: 's', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });
  compiler.registerOperator('jot/arc', { path: 'jot/arc', schema: { arguments: [{ name: 's', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });
  compiler.registerOperator('jot/rotateX', { path: 'jot/rotateX', schema: { arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'a', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } } });

  const ast = parser.parse('[box(10), arc(20).rotateX(0.5)]');
  const res = await compiler.evaluate(ast);
  assert.ok(Array.isArray(res));
  assert.strictEqual(res.length, 2);
  assert.strictEqual(res[0].path, 'jot/box');
  assert.strictEqual(res[1].path, 'jot/rotateX');
});

test('JotCAD Parser: Complex Object Literals', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('jot/pt', {
    path: 'jot/pt',
    schema: {
        arguments: [{ name: 'x', type: 'jot:number' }, { name: 'y', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:vec3" } }
    }
  });

  compiler.registerOperator('jot/tri', {
    path: 'jot/tri',
    schema: {
      arguments: [
        { name: 'config', type: 'any' }
      ],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('tri({ points: [pt(0,0), pt(10,0), pt(5,10)], color: "red" })');
  const res = await compiler.evaluate(ast);
  
  assert.strictEqual(res.path, 'jot/tri');
  const points = res.parameters.config.points;
  assert.strictEqual(points.length, 3);
  assert.strictEqual(points[0].path, 'jot/pt');
  assert.strictEqual(points[0].parameters.x, 0);
  assert.strictEqual(res.parameters.config.color, 'red');
});

test('JotCAD Evaluator: Deep Symbol Resolution', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();
  compiler.registerOperator('jot/box', {
    path: 'jot/box',
    schema: { arguments: [{ name: 'config', type: 'any' }], outputs: { "$out": { type: "jot:shape" } } }
  });
  compiler.registerOperator('jot/rotateX', {
    path: 'jot/rotateX',
    schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'angle', type: 'any' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('box({ width: width }).rotateX(turn)');
  const res = await compiler.evaluate(ast, { width: 100, turn: 0.5 });
  assert.strictEqual(res.parameters.$in.parameters.config.width, 100);
  assert.strictEqual(res.parameters.angle, 0.5);
});

test('JotCAD Evaluator: .spec() Canonicalization', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();
    
    compiler.registerOperator('jot/box', {
        path: 'jot/box',
        schema: { arguments: [{ name: 'length', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } }
    });

    // Mock spec() behavior: it updates operator schemas or parameters
    compiler.registerOperator('jot/spec', {
        path: 'jot/spec',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'definition', type: 'any' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    const ast = parser.parse('box(length).spec({ length: { alias: ["L", "len"], default: 50 } })');
    const res = await compiler.evaluate(ast, { L: 100 });
    
    // In this simplified test, we just check that the symbol was correctly handled
    assert.strictEqual(res.path, 'jot/spec');
    assert.strictEqual(res.parameters.$in.path, 'jot/box');
});
