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
        arguments: [{ name: 'diameter', type: 'jot:number', affiliate: '$out' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('Triangle', {
    path: 'jot/Triangle/equilateral',
    schema: {
        arguments: [{ name: 'size', type: 'jot:number', affiliate: '$out' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('eachCorner', {
    path: 'jot/eachCorner',
    schema: {
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('cut', {
    path: 'jot/cut',
    schema: {
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' }, 
            { name: 'tools', type: 'jot:shapes' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('at', {
    path: 'jot/at',
    schema: {
      arguments: [
        { name: '$in', type: 'jot:shape', affiliate: '$out' },
        { name: 'target', type: 'jot:op<$in:shape, $out:shape>' },
        { name: 'op', type: 'jot:op<$in:shape, $out:shape>' }
      ],
      outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const ast = parser.parse('Hexagon(30).at(eachCorner(), cut(Triangle(2)))');
  const res = await compiler.evaluate(ast);
  
  const at = res[0];
  assert.strictEqual(at.path, 'jot/at');
  assert.strictEqual(at.parameters.target.path, 'jot/eachCorner');
  
  // eachCorner should have inherited the Hexagon(30) subject
  assert.strictEqual(at.parameters.target.parameters.$in.path, 'jot/Hexagon/full');

  const cut = at.parameters.op;
  assert.strictEqual(cut.path, 'jot/cut');
  assert.strictEqual(cut.parameters.tools[0].path, 'jot/Triangle/equilateral');

  // cut should also have inherited the Hexagon(30) subject
  assert.strictEqual(cut.parameters.$in.path, 'jot/Hexagon/full');
});
