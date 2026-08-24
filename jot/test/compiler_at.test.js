import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

// FAIL: Fails with TypeError because HOLE constant was removed and unbound 
// parameters are now undefined.
test('Compiler: Hexagon(30).at(eachCorner(), cut(Triangle(2)))', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Hexagon', {
    path: 'jot/Hexagon/full',
    schema: {
        arguments: [{ name: 'diameter', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('Triangle', {
    path: 'jot/Triangle/equilateral',
    schema: {
        arguments: [{ name: 'size', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('eachCorner', {
    path: 'jot/eachCorner',
    schema: {
        inputs: { '$in': { type: 'jot:shape' } },
        arguments: [],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('cut', {
    path: 'jot/cut',
    schema: {
        inputs: { '$in': { type: 'jot:shape' } },
        arguments: [
            { name: 'tools', type: 'jot:shapes' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('at', {
    path: 'jot/at',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [
        { name: 'target', type: 'jot:op<$in:jot:shape, $out:jot:shape>' },
        { name: 'op', type: 'jot:op<$in:jot:shape, $out:jot:shape>' }
      ],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('Hexagon(30).at(eachCorner(), cut(Triangle(2))) -> $out');
  const schema = { outputs: { "$out": { type: "jot:shape" } } };
  const res = await compiler.evaluate(ast, {}, schema);
  
  const at = res[0].selector;
  assert.strictEqual(at.path, 'jot/at');
  assert.strictEqual(at.parameters.target.path, 'jot/eachCorner');
  
  // In higher-order operations (at, snap, on), $in is bound at the top-level
  assert.strictEqual(at.parameters.$in.path, 'jot/Hexagon/full');
  assert.strictEqual(at.parameters.$in.parameters.diameter, 30);

  const cut = at.parameters.op;
  assert.strictEqual(cut.path, 'jot/cut');
  assert.strictEqual(cut.parameters.tools[0].path, 'jot/Triangle/equilateral');
  assert.strictEqual(cut.parameters.tools[0].parameters.size, 2);
  // Recipe $in remains unbound at compile-time for dynamic kernel execution
  assert.strictEqual(cut.parameters.$in, undefined);
});
