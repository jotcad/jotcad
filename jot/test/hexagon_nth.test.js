import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Hexagon Points Nth Regression', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    // Register necessary ops for the repro
    compiler.registerOperator('Hexagon', {
        path: 'jot/Hexagon/edgeToEdge',
        schema: {
            arguments: [
                { name: 'edgeToEdge', type: 'jot:number' },
                { name: 'turns', type: 'jot:number', default: 0 }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('asPoints', {
        path: 'jot/asPoints',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('points', {
        path: 'jot/points',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('nth', {
        path: 'jot/nth',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'index', type: 'jot:numbers' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    await t.test('Hexagon with edgeToEdge should resolve correctly', async () => {
        const script = 'Hexagon(edgeToEdge=248) -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        assert.strictEqual(res[0].selector.path, 'jot/Hexagon/edgeToEdge');
        assert.strictEqual(res[0].selector.parameters.edgeToEdge, 248);
    });

    await t.test('asPoints() returns a cloud (0 components), causing nth to fail', async () => {
        const script = 'Hexagon(edgeToEdge=248).asPoints().nth(1) -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        const sel = res[0].selector;
        assert.strictEqual(sel.path, 'jot/nth');
        assert.strictEqual(sel.parameters.$in.path, 'jot/asPoints');
    });

    await t.test('points() returns components, allowing nth to succeed', async () => {
        const script = 'Hexagon(edgeToEdge=248).points().nth(1) -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        assert.strictEqual(res[0].selector.path, 'jot/nth');
        assert.strictEqual(res[0].selector.parameters.$in.path, 'jot/points');
    });
});
