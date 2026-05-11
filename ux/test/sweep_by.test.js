import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

describe('SweepBy Syntax', () => {
  const parser = new JotParser();
  const mockVfs = { readData: async () => null };
  const compiler = new JotCompiler(mockVfs, { optimizeAliases: true });

  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: { arguments: [{ name: 'width', type: 'jot:number' }], outputs: { $out: { type: 'jot:shape' } } },
  });
  
  compiler.registerOperator('Circle', {
    path: 'jot/Circle',
    schema: { arguments: [{ name: 'radius', type: 'jot:number' }], outputs: { $out: { type: 'jot:shape' } } },
  });

  compiler.registerOperator('sweepBy', {
    path: 'jot/sweepBy',
    schema: { 
      arguments: [
        { name: '$in', type: 'jot:shape', affiliate: '$out' },
        { name: 'profile', type: 'jot:shape' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    },
  });

  it('should compile path.sweepBy(profile)', async () => {
    const code = 'Box(100).sweepBy(Circle(5)) -> $out';
    const ast = parser.parse(code);
    const schema = { outputs: { $out: { type: 'jot:shape' } } };
    const results = await compiler.evaluate(ast, {}, schema);
    const res = results[0];

    expect(res.path).toBe('jot/sweepBy');
    expect(res.parameters.$in.path).toBe('jot/Box');
    expect(res.parameters.profile.path).toBe('jot/Circle');
  });
});
