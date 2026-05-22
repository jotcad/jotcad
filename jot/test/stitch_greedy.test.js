import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Stitch Operator: Greedy Argument Consumption', async (t) => {
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

    await t.test('Greedy syntax start=0, 10 should resolve to [0, 10]', async () => {
        // This exercises the compiler's ability to pull subsequent unnamed numbers into a plural argument
        const script = "Link(Point(0,0,0), Point(100,0,0)).stitch(start=0, 10, end=0, 20).color('red') -> $out";
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        
        const sel = res[0].selector;
        assert.strictEqual(sel.path, 'jot/color');
        const stitch = sel.parameters.$in;
        
        // VERIFICATION OF GREEDY CONSUMPTION
        assert.deepStrictEqual(stitch.parameters.start, [0, 10], 'start should consume the following 10');
        assert.deepStrictEqual(stitch.parameters.end, [0, 20], 'end should consume the following 20');
        assert.deepStrictEqual(stitch.parameters.repeat, [], 'repeat should remain empty default');
    });
});
