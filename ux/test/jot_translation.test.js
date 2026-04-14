import { describe, it, expect } from 'vitest';
import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

describe('Jot Dynamic Compilation', () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    // Setup dynamic schemas
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

    compiler.registerOperator('offset', {
        path: 'op/offset',
        schema: {
            properties: {
                radius: { type: 'number', default: 1 }
            }
        }
    });

    compiler.registerOperator('rx', {
        path: 'op/rotateX',
        schema: { properties: { turns: { type: 'number' } } }
    });

    const compile = (code) => {
        const ast = parser.parse(code);
        return compiler.evaluate(ast);
    };

    describe('Dynamic Positional Mapping', () => {
        it('should map box(1, 2, 3) using schema', () => {
            const selector = compile('box(1, 2, 3)');
            expect(selector).toEqual({
                path: 'shape/box',
                parameters: { width: 1, height: 2, depth: 3 }
            });
        });

        it('should apply defaults: box(100)', () => {
            const selector = compile('box(100)');
            expect(selector).toEqual({
                path: 'shape/box',
                parameters: { width: 100, height: 10, depth: 10 }
            });
        });
    });

    describe('Dynamic Chaining', () => {
        it('should handle method chaining: box(10).offset(2)', () => {
            const selector = compile('box(10).offset(2)');
            expect(selector).toEqual({
                path: 'op/offset',
                parameters: {
                    radius: 2,
                    source: {
                        path: 'shape/box',
                        parameters: { width: 10, height: 10, depth: 10 }
                    }
                }
            });
        });
    });

    describe('Universal Sequence Principle', () => {
        it('should handle Implicit Mapping: [box(10), box(20)].offset(2)', () => {
            const selector = compile('[box(10), box(20)].offset(2)');
            expect(selector).toHaveLength(2);
            expect(selector[0].path).toBe('op/offset');
            expect(selector[0].parameters.source.parameters.width).toBe(10);
            expect(selector[1].parameters.source.parameters.width).toBe(20);
        });

        it('should handle Cartesian Product: box(10).rx([0, 0.25])', () => {
            const selector = compile('box(10).rx([0, 0.25])');
            expect(selector).toHaveLength(2);
            expect(selector[0].parameters.turns).toBe(0);
            expect(selector[1].parameters.turns).toBe(0.25);
        });
    });

    describe('Routing and Variants', () => {
        it('should route hexagon variants: hexagon({ variant: "half", radius: 50 })', () => {
            compiler.registerOperator('hexagon', {
                path: 'shape/hexagon',
                schema: { properties: { radius: { type: 'number' }, variant: { type: 'string' } } }
            });
            const selector = compile('hexagon({ variant: "half", radius: 50 })');
            expect(selector.path).toBe('shape/hexagon/half');
            expect(selector.parameters.radius).toBe(50);
        });
    });
});
