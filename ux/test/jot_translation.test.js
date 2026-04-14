import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

describe('Jot Dynamic Compilation (Next Gen)', () => {
    const parser = new JotParser();
    
    // Mock VFS for testing generator unrolling
    const mockVfs = {
        readData: async (path, params) => {
            if (path === 'op/range') {
                const { start = 0, stop = params.arg || 0, step = 1 } = params;
                const res = [];
                for (let i = start; i < stop; i += step) res.push(i);
                return res;
            }
            if (path === 'op/iota') {
                const count = params.count || params.arg || 0;
                return Array.from({ length: count }, (_, i) => i);
            }
            return null;
        }
    };

    const compiler = new JotCompiler(mockVfs);

    // Setup dynamic schemas
    compiler.registerOperator('range', {
        path: 'op/range',
        returns: { type: 'array' }
    });

    compiler.registerOperator('iota', {
        path: 'op/iota',
        returns: { type: 'array' }
    });

    compiler.registerOperator('box', {
        path: 'shape/box',
        schema: {
            properties: {
                width: { type: 'number', default: 10 },
                height: { type: 'number', default: 10 },
                depth: { type: 'number', default: 10 }
            }
        }
    });

    compiler.registerOperator('orb', {
        path: 'shape/orb',
        schema: { properties: { diameter: { type: 'number', default: 10 } } }
    });

    compiler.registerOperator('offset', {
        path: 'op/offset',
        schema: { properties: { radius: { type: 'number', default: 1 } } }
    });

    const compile = async (code) => {
        const ast = parser.parse(code);
        return await compiler.evaluate(ast);
    };

    describe('Primacy of Diameter', () => {
        it('should map radius to diameter: orb({ radius: 5 })', async () => {
            const selector = await compile('orb({ radius: 5 })');
            expect(selector.parameters.diameter).toBe(10);
            expect(selector.parameters.radius).toBeUndefined();
        });

        it('should map apothem to diameter for hexagon', async () => {
            compiler.registerOperator('hexagon', { path: 'shape/hexagon' });
            const selector = await compile('hexagon({ apothem: 10 })');
            // d = 2 * a / cos(30) = 20 / 0.866... = 23.094
            expect(selector.parameters.diameter).toBeCloseTo(23.094, 3);
        });
    });

    describe('Generator Unrolling (Op-Based)', () => {
        it('should unroll range(3) into data', async () => {
            const res = await compile('range(3)');
            expect(res).toEqual([0, 1, 2]);
        });

        it('should handle range used as argument: box(range(3))', async () => {
            const res = await compile('box(range(3))');
            expect(res).toHaveLength(3);
            expect(res[0].parameters.width).toBe(0);
            expect(res[1].parameters.width).toBe(1);
            expect(res[2].parameters.width).toBe(2);
        });

        it('should handle iota used in chaining: iota(2).offset(1)', async () => {
            // This unrolls iota(2) -> [0, 1] then maps offset(1) over it
            const res = await compile('iota(2).offset(1)');
            expect(res).toHaveLength(2);
            expect(res[0].path).toBe('op/offset');
            expect(res[0].parameters.$in).toBe(0);
            expect(res[1].parameters.$in).toBe(1);
        });
    });

    describe('Universal Sequence Principle', () => {
        it('should handle Cartesian Product with diameter: box([10, 20])', async () => {
            const res = await compile('box([10, 20])');
            expect(res).toHaveLength(2);
            expect(res[0].parameters.width).toBe(10);
            expect(res[1].parameters.width).toBe(20);
        });
    });
});
