import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

describe('Jot Compiler Selector Injection', () => {
  const parser = new JotParser();

  // Mock VFS that returns a Selector for 'range'
  const mockVfs = {
    readData: async (path, params) => {
      // In a real system, range() would return a Selector that eventually resolves to an array.
      // Here we simulate the compiler's output for range(5).
      if (path === 'jot/range') return [0, 1, 2, 3, 4];
      return null;
    },
  };

  const compiler = new JotCompiler(mockVfs);

  // Register Box and Range
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [
        { name: 'width', type: 'jot:number', default: 10 },
        { name: 'height', type: 'jot:number', default: 10 }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('Rotate', {
    path: 'jot/Rotate',
    schema: {
      arguments: [
        { name: 'turns', type: 'jot:numbers' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('range', {
    path: 'jot/range',
    schema: {
        arguments: [{ name: 'count', type: 'jot:number' }],
        outputs: { $out: { type: 'jot:numbers' } }
    }
  });

  it('successfully injects a Selector into a jot:number argument', async () => {
    const code = 'Box(width = range(5)) -> $out';
    const ast = parser.parse(code);
    
    const artifacts = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
    const result = artifacts.find(a => a.port === '$out').selector;
    console.log('Result:', JSON.stringify(result, null, 2));
    
    // It should be a Selector-like object (or formal Selector instance)
    expect(result.parameters.width).toMatchObject({
        path: 'jot/range',
        output: '$out'
    });
  });

  it('successfully handles a mixed pool of literals and Selectors in jot:numbers', async () => {
    const code = 'Rotate(turns = [0.1, range(3), 0.5]) -> $out';
    const ast = parser.parse(code);

    const artifacts = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
    const result = artifacts.find(a => a.port === '$out').selector;
    console.log('Mixed Result:', JSON.stringify(result, null, 2));

    expect(result.parameters.turns).toHaveLength(3);
    expect(result.parameters.turns[0]).toBe(0.1);
    expect(result.parameters.turns[1]).toMatchObject({ path: 'jot/range' });
    expect(result.parameters.turns[2]).toBe(0.5);
  });
});
