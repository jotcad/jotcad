import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

describe('Jot Dynamic Compilation (Case Sensitive)', () => {
    const parser = new JotParser();
    
    const mockVfs = {
        readData: async (path, params) => {
            if (path === 'op/range') return [0, 1, 2];
            return null;
        }
    };

    const compiler = new JotCompiler(mockVfs, { optimizeAliases: true });

    // Setup dynamic schemas centrally
    const setupOperators = (c) => {
        c.registerOperator('Box', {
            path: 'shape/box',
            schema: { arguments: { width: { type: 'number', default: 10 } } }
        });
        c.registerOperator('Orb', {
            path: 'shape/orb',
            schema: { arguments: { diameter: { type: 'number', default: 10 } } }
        });
        c.registerOperator('Hexagon', {
            path: 'shape/hexagon/full',
            schema: { arguments: { diameter: { type: 'number' } } }
        });
        c.registerOperator('offset', {
            path: 'op/offset',
            schema: { arguments: { radius: { type: 'number', default: 1 } } }
        });
        c.registerOperator('outline', {
            path: 'op/outline',
            schema: { 
                arguments: { "$in": { type: 'shape' } },
                outputs: { "$out": { type: 'shape' } }
            }
        });
        c.registerOperator('pdf', {
            path: 'op/pdf',
            schema: {
                arguments: { "$in": { type: 'shape' }, "path": { type: 'string' } }
            },
            metadata: { aliases: { "$out": "$in" } }
        });
        c.registerOperator('range', {
            path: 'op/range',
            returns: { type: 'array' }
        });
    };

    setupOperators(compiler);

    const compile = async (code, options = {}) => {
        const ast = parser.parse(code);
        const c = options.compiler || compiler;
        return await c.evaluate(ast);
    };

    describe('Strict Casing', () => {
        it('should resolve PascalCase Constructor: Box(10)', async () => {
            const selector = await compile('Box(10)');
            expect(selector.path).toBe('shape/box');
        });

        it('should NOT resolve lowercase constructor if not registered: box(10)', async () => {
            const selector = await compile('box(10)');
            expect(selector.path).toBe('op/box');
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

            const res = await compile('Box(10).pdf("out.pdf")', { compiler: compilerRaw });
            expect(res.path).toBe('op/pdf');
        });

        it('should handle optimized chain (optimizeAliases: true)', async () => {
            const res = await compile('Box(10).pdf("out.pdf")');
            expect(res).toHaveLength(2);
            expect(res[0].path).toBe('shape/box');
            expect(res[1].path).toBe('op/pdf');
        });

        it('should handle complex optimized chain: Box(10).pdf("out.pdf").offset(2)', async () => {
            const res = await compile('Box(10).pdf("out.pdf").offset(2)');
            expect(res).toHaveLength(2);
            expect(res[0].path).toBe('op/offset');
            expect(res[1].path).toBe('op/pdf');
        });
    });

    describe('Generator Unrolling', () => {
        it('should unroll range(3) using PascalCase: Box(range(3))', async () => {
            const res = await compile('Box(range(3))');
            expect(res).toHaveLength(3);
            expect(res[0].parameters.width).toBe(0);
            expect(res[2].parameters.width).toBe(2);
        });
    });

    describe('Comprehensive End-to-End Chains', () => {
        it('should handle Hexagon(50).offset(2).outline().pdf("out.pdf")', async () => {
            const res = await compile('Hexagon(50).offset(2).outline().pdf("out.pdf")');
            expect(res).toHaveLength(2);
            expect(res[0].path).toBe('op/outline'); 
            expect(res[1].path).toBe('op/pdf');
        });
    });

    describe('Primacy of Diameter', () => {
        it('should map radius to diameter for PascalCase Orb', async () => {
            const selector = await compile('Orb({ radius: 5 })');
            expect(selector.parameters.diameter).toBe(10);
        });
    });
});
