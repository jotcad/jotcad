import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Compiler: Hexagon(30).at(eachCorner(), cut(Triangle(2)))', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  compiler.registerOperator('Hexagon', {
    path: 'jot/Hexagon/full',
    schema: { arguments: [{ name: 'diameter', type: 'jot:number' }] }
  });

  compiler.registerOperator('Triangle', {
    path: 'jot/Triangle/equilateral',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }] }
  });

  compiler.registerOperator('eachCorner', {
    path: 'jot/eachCorner',
    schema: { arguments: [{ name: '$in', type: 'jot:shape' }] }
  });

  compiler.registerOperator('cut', {
    path: 'jot/cut',
    schema: { arguments: [{ name: '$in', type: 'jot:shape' }, { name: 'tools', type: 'jot:shapes' }] }
  });

  compiler.registerOperator('at', {
    path: 'jot/at',
    schema: {
      arguments: [
        { name: '$in', type: 'jot:shape' },
        { name: 'target', type: 'jot:shape' },
        { name: 'op', type: 'jot:operation' }
      ]
    }
  });

  const ast = parser.parse('Hexagon(30).at(eachCorner(), cut(Triangle(2)))');
  const result = await compiler.evaluate(ast);
  
  console.log(JSON.stringify(result, null, 2));
});
