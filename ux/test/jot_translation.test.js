import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

describe('Jot Dynamic Compilation (Case Sensitive)', () => {
  const parser = new JotParser();

  const mockVfs = {
    readData: async (path, params) => {
      if (path === 'op/range') return [0, 0.1, 0.2];
      return null;
    },
  };

  const baseCompiler = new JotCompiler(mockVfs, { optimizeAliases: true });

  // Setup dynamic schemas centrally
  const setupOperators = (c) => {
    c.registerOperator('Box', {
      path: 'jot/Box',
      schema: { 
        arguments: [
          { name: 'width', type: 'jot:number', default: 10 },
          { name: 'height', type: 'jot:number', default: 10 },
          { name: 'depth', type: 'jot:number', default: 0 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('Disk', {
      path: 'jot/Disk',
      schema: { 
        arguments: [
          { name: 'diameter', type: 'jot:number', default: 10 },
          { name: 'width', type: 'jot:number', default: 0 },
          { name: 'height', type: 'jot:number', default: 0 },
          { name: 'start', type: 'jot:number', default: 0 },
          { name: 'end', type: 'jot:number', default: 1 },
          { name: 'zag', type: 'jot:number', default: 0.1 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('Arc', {
      path: 'jot/Arc',
      schema: { 
        arguments: [
          { name: 'diameter', type: 'jot:number', default: 10 },
          { name: 'width', type: 'jot:number', default: 0 },
          { name: 'height', type: 'jot:number', default: 0 },
          { name: 'start', type: 'jot:number', default: 0 },
          { name: 'end', type: 'jot:number', default: 1 },
          { name: 'zag', type: 'jot:number', default: 0.1 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });    c.registerOperator('Orb', {
      path: 'jot/Orb',
      schema: { 
        arguments: [
          { name: 'diameter', type: 'jot:number', default: 10 },
          { name: 'width', type: 'jot:number', default: 0 },
          { name: 'height', type: 'jot:number', default: 0 },
          { name: 'depth', type: 'jot:number', default: 0 },
          { name: 'zag', type: 'jot:number', default: 0.1 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('Hexagon', {
      path: 'jot/Hexagon/full',
      schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('offset', {
      path: 'jot/offset',
      schema: { 
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' },
            { name: 'radius', type: 'jot:number', default: 1 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('rotateZ', {
      path: 'jot/rotateZ',
      schema: { 
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' },
            { name: 'turns', type: 'jot:numbers', default: [0] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('outline', {
      path: 'jot/outline',
      schema: {
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } },
      },
    });
    c.registerOperator('pdf', {
      path: 'jot/pdf',
      schema: {
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'path', type: 'jot:string' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
      metadata: { aliases: { $out: '$in' } },
    });
    c.registerOperator('box', {
      path: 'jot/box',
      schema: { 
        arguments: [{ name: 'width', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('arc', {
      path: 'jot/arc',
      schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('X', {
      path: 'jot/X',
      schema: { 
        arguments: [{ name: 'pos', type: 'jot:number', default: 0 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('eachCorner', {
      path: 'jot/eachCorner',
      schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('edges', {
      path: 'jot/edges',
      schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('eachEdge', {
      path: 'jot/eachEdge',
      schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('move', {
      path: 'jot/move',
      schema: { 
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: 'x', type: 'jot:number', default: 0 },
          { name: 'y', type: 'jot:number', default: 0 },
          { name: 'z', type: 'jot:number', default: 0 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('cut', {
      path: 'jot/cut',
      schema: { 
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: 'tools', type: 'jot:shapes', default: [] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('join', {
      path: 'jot/join',
      schema: { 
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: 'tools', type: 'jot:shapes', default: [] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('clip', {
      path: 'jot/clip',
      schema: { 
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: 'tools', type: 'jot:shapes', default: [] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('fuse', {
      path: 'jot/fuse',
      schema: { 
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: 'tools', type: 'jot:shapes', default: [] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('disjoint', {
      path: 'jot/disjoint',
      schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('range', {
      path: 'jot/range',
      schema: {
        arguments: [{ name: 'count', type: 'number' }],
        outputs: { $out: { type: 'jot:numbers' } }
      },
      returns: { type: 'array' },
    });
  };

  setupOperators(baseCompiler);

  const compile = async (code, options = {}) => {
    const ast = parser.parse(code);
    const c = options.compiler || baseCompiler;
    const results = await c.evaluate(ast);
    return results[0];
  };

  describe('Strict Casing', () => {
    it('should resolve PascalCase Constructor: Box(10)', async () => {
      const selector = await compile('Box(10)');
      expect(selector.path).toBe('jot/Box');
    });

    it('should resolve lowercase constructor if registered: box(10)', async () => {
      const selector = await compile('box(10)');
      expect(selector.path).toBe('jot/box');
    });

    it('should resolve camelCase operation: Box(10).offset(2)', async () => {
      const selector = await compile('Box(10).offset(2)');
      expect(selector.path).toBe('jot/offset');
      expect(selector.parameters.$in.path).toBe('jot/Box');
    });
  });

  describe('Alias Resolution (Optimization Modes)', () => {
    it('should handle unoptimized chain (optimizeAliases: false)', async () => {
      const compilerRaw = new JotCompiler(mockVfs, { optimizeAliases: false });
      setupOperators(compilerRaw);

      const res = await compile('Box(10).pdf("out.pdf")', {
        compiler: compilerRaw,
      });
      expect(res.path).toBe('jot/pdf');
    });

    it('should handle optimized chain (optimizeAliases: true)', async () => {
      const res = await compile('Box(10).pdf("out.pdf")');
      // In next-gen compiler, if a method is a side-demand (like pdf), 
      // evaluate returns the primary result (the tail of the chain).
      expect(res.path).toBe('jot/pdf');
    });

    it('should handle complex optimized chain: Box(10).pdf("out.pdf").offset(2)', async () => {
      const res = await compile('Box(10).pdf("out.pdf").offset(2)');
      expect(res.path).toBe('jot/offset');
      expect(res.parameters.$in.path).toBe('jot/pdf');
    });
  });

  describe('Generator Unrolling', () => {
    it('should pass range(3) as a Selector to rotateZ: Box(10).rotateZ(range(3))', async () => {
      const res = await compile('Box(10).rotateZ(range(3))');
      expect(res.path).toBe('jot/rotateZ');
      expect(res.parameters.turns[0].path).toBe('jot/range');
      expect(res.parameters.turns[0].parameters.count).toBe(3);
    });
  });

  describe('Comprehensive End-to-End Chains', () => {
    it('should handle Hexagon(50).offset(2).outline().pdf("out.pdf")', async () => {
      const res = await compile(
        'Hexagon(50).offset(2).outline().pdf("out.pdf")'
      );
      expect(res.path).toBe('jot/pdf');
    });
  });

  describe('Standard Diameter', () => {
    it('should use diameter for PascalCase Orb', async () => {
      const selector = await compile('Orb(diameter = 10)');
      expect(selector.parameters.diameter).toBe(10);
    });
  });
});
