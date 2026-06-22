import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Stitch Operator Regression: start=[0,10], end=[0,20]', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    // Register necessary ops
    compiler.registerOperator('Point', {
        path: 'jot/Point',
        schema: {
            arguments: [{ name: 'x', type: 'jot:number' }, { name: 'y', type: 'jot:number' }, { name: 'z', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('Link', {
        path: 'jot/Link',
        schema: {
            arguments: [
                { name: 'shapes', type: 'jot:shapes' },
                { name: 'smooth', type: 'jot:boolean', default: false },
                { name: 'zag', type: 'jot:number', default: 0 }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('LinkCurve', {
        path: 'jot/LinkCurve',
        schema: {
            arguments: [
                { name: 'shapes', type: 'jot:shapes' },
                { name: 'zag', type: 'jot:number', default: 0 }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('LoopCurve', {
        path: 'jot/LoopCurve',
        schema: {
            arguments: [
                { name: 'shapes', type: 'jot:shapes' },
                { name: 'zag', type: 'jot:number', default: 0 }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('Rule', {
        path: 'jot/Rule',
        schema: {
            inputs: {},
            arguments: [
                { name: '$a', type: 'jot:shape' },
                { name: '$b', type: 'jot:shape' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('rule', {
        path: 'jot/rule',
        schema: {
            inputs: {
                '$in': { type: 'jot:shape', affiliate: '$out' }
            },
            arguments: [
                { name: '$b', type: 'jot:shape' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('stitch', {
        path: 'jot/stitch',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'repeat', type: 'jot:numbers', default: [] },
                { name: 'start', type: 'jot:numbers', default: [] },
                { name: 'end', type: 'jot:numbers', default: [] },
                { name: 'offset', type: 'jot:number', default: 0 }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('color', {
        path: 'jot/color',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [{ name: 'name', type: 'jot:string' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    await t.test('Complex Stitch Pattern should produce correct selector', async () => {
        const script = "Link(Point(0,0,0), Point(100,0,0)).stitch(start=[0, 10], end=[0, 20]).color('red') -> $out";
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        const sel = res[0].selector;
        assert.strictEqual(sel.path, 'jot/color');
        const stitch = sel.parameters.$in;
        assert.strictEqual(stitch.path, 'jot/stitch');
        assert.deepStrictEqual(stitch.parameters.start, [0, 10]);
        assert.deepStrictEqual(stitch.parameters.end, [0, 20]);
    });

    await t.test('Link with raw vector coordinate promotion', async () => {
        const script = "Link([0,0,0], [1, 0, 1], [1, 0, 4], [2, 0, 5], smooth = true) -> $out";
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        const sel = res[0].selector;
        assert.strictEqual(sel.path, 'jot/Link');
        assert.strictEqual(sel.parameters.smooth, true);
        assert.ok(Array.isArray(sel.parameters.shapes));
        assert.strictEqual(sel.parameters.shapes.length, 4);
        assert.strictEqual(sel.parameters.shapes[0].path, 'jot/Point');
        assert.strictEqual(sel.parameters.shapes[0].output, '$out');
        assert.strictEqual(sel.parameters.shapes[0].parameters.x, 0);
        assert.strictEqual(sel.parameters.shapes[0].parameters.y, 0);
        assert.strictEqual(sel.parameters.shapes[0].parameters.z, 0);
        assert.strictEqual(sel.parameters.shapes[3].path, 'jot/Point');
        assert.strictEqual(sel.parameters.shapes[3].output, '$out');
        assert.strictEqual(sel.parameters.shapes[3].parameters.x, 2);
        assert.strictEqual(sel.parameters.shapes[3].parameters.y, 0);
        assert.strictEqual(sel.parameters.shapes[3].parameters.z, 5);
    });

    await t.test('LinkCurve with raw vector coordinate promotion', async () => {
        const script = "LinkCurve([0,0,0], [1, 0, 1], [1, 0, 4], [2, 0, 5]) -> $out";
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        const sel = res[0].selector;
        assert.strictEqual(sel.path, 'jot/LinkCurve');
        assert.ok(Array.isArray(sel.parameters.shapes));
        assert.strictEqual(sel.parameters.shapes.length, 4);
        assert.strictEqual(sel.parameters.shapes[0].path, 'jot/Point');
        assert.strictEqual(sel.parameters.shapes[0].output, '$out');
        assert.strictEqual(sel.parameters.shapes[3].path, 'jot/Point');
        assert.strictEqual(sel.parameters.shapes[3].output, '$out');
    });

    await t.test('Rule and rule operators resolution and mapping', async () => {
        // Constructor Rule
        const script1 = "Rule(Point(0,0,0), Point(10,10,10)) -> $out";
        const ast1 = parser.parse(script1);
        const res1 = await compiler.evaluate(ast1, {}, { outputs: { $out: 'jot:shape' } });
        assert.strictEqual(res1[0].selector.path, 'jot/Rule');
        assert.strictEqual(res1[0].selector.parameters.$a.path, 'jot/Point');
        assert.strictEqual(res1[0].selector.parameters.$b.path, 'jot/Point');
        
        // Method rule
        const script2 = "Point(0,0,0).rule(Point(10,10,10)) -> $out";
        const ast2 = parser.parse(script2);
        const res2 = await compiler.evaluate(ast2, {}, { outputs: { $out: 'jot:shape' } });
        assert.strictEqual(res2[0].selector.path, 'jot/rule');
        assert.strictEqual(res2[0].selector.parameters.$in.path, 'jot/Point');
        assert.strictEqual(res2[0].selector.parameters.$b.path, 'jot/Point');
    });
});
