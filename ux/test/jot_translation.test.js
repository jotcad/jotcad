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
      path: 'shape/box',
      schema: { 
        arguments: [{ name: 'width', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('Orb', {
      path: 'shape/orb',
      schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('Hexagon', {
      path: 'shape/hexagon/full',
      schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('offset', {
      path: 'op/offset',
      schema: { 
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' },
            { name: 'radius', type: 'jot:number', default: 1 }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('rotateZ', {
      path: 'op/rotate',
      schema: { 
        arguments: [
            { name: '$in', type: 'jot:shape', affiliate: '$out' },
            { name: 'angle', type: 'jot:numbers', default: [0] }
        ],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('outline', {
      path: 'op/outline',
      schema: {
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } },
      },
    });
    c.registerOperator('pdf', {
      path: 'op/pdf',
      schema: {
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'path', type: 'jot:string' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
      metadata: { aliases: { $out: '$in' } },
    });
    c.registerOperator('box', {
      path: 'shape/box',
      schema: { 
        arguments: [{ name: 'width', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('arc', {
      path: 'shape/arc',
      schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number', default: 10 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('X', {
      path: 'shape/axis',
      schema: { 
        arguments: [{ name: 'pos', type: 'jot:number', default: 0 }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('eachCorner', {
      path: 'op/corners',
      schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }],
        outputs: { $out: { type: 'jot:shape' } }
      },
    });
    c.registerOperator('range', {
      path: 'op/range',
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
    return await c.evaluate(ast);
  };

  describe('Strict Casing', () => {
    it('should resolve PascalCase Constructor: Box(10)', async () => {
      const selector = await compile('Box(10)');
      expect(selector.path).toBe('shape/box');
    });

    it('should resolve lowercase constructor if registered: box(10)', async () => {
      const selector = await compile('box(10)');
      expect(selector.path).toBe('shape/box');
    });

    it('should resolve camelCase operation: Box(10).offset(2)', async () => {
      const selector = await compile('Box(10).offset(2)');
      expect(selector.path).toBe('op/offset');
      expect(selector.parameters.$in.path).toBe('shape/box');
    });
  });

  describe('Alias Resolution (Optimization Modes)', () => {
    it('should handle unoptimized chain (optimizeAliases: false)', async () => {
      const compilerRaw = new JotCompiler(mockVfs, { optimizeAliases: false });
      setupOperators(compilerRaw);

      const res = await compile('Box(10).pdf("out.pdf")', {
        compiler: compilerRaw,
      });
      expect(res.path).toBe('op/pdf');
    });

    it('should handle optimized chain (optimizeAliases: true)', async () => {
      const res = await compile('Box(10).pdf("out.pdf")');
      // In next-gen compiler, if a method is a side-demand (like pdf), 
      // evaluate returns the primary result.
      expect(res.path).toBe('shape/box');
    });

    it('should handle complex optimized chain: Box(10).pdf("out.pdf").offset(2)', async () => {
      const res = await compile('Box(10).pdf("out.pdf").offset(2)');
      expect(res.path).toBe('op/offset');
      expect(res.parameters.$in.path).toBe('shape/box');
    });
  });

  describe('Generator Unrolling', () => {
    it('should pass range(3) as a Selector to rotateZ: Box(10).rotateZ(range(3))', async () => {
      const res = await compile('Box(10).rotateZ(range(3))');
      expect(res.path).toBe('op/rotate');
      expect(res.parameters.angle[0].path).toBe('op/range');
      expect(res.parameters.angle[0].parameters.count).toBe(3);
    });
  });

  describe('Comprehensive End-to-End Chains', () => {
    it('should handle Hexagon(50).offset(2).outline().pdf("out.pdf")', async () => {
      const res = await compile(
        'Hexagon(50).offset(2).outline().pdf("out.pdf")'
      );
      expect(res.path).toBe('op/outline');
    });
  });

  describe('Standard Diameter', () => {
    it('should use diameter for PascalCase Orb', async () => {
      const selector = await compile('Orb(diameter = 10)');
      expect(selector.parameters.diameter).toBe(10);
    });
  });
});
