import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Compiler: dup Operator', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'size', type: 'jot:number' }],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('dup', {
        path: 'jot/dup',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [{ name: 'count', type: 'jot:number', default: 1 }],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    await t.test('Box(10).dup(3) -> $out', async () => {
        const script = 'Box(10).dup(3) -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        const sel = res[0].selector;

        assert.strictEqual(sel.path, 'jot/dup');
        assert.strictEqual(sel.parameters.count, 3);
        assert.strictEqual(sel.parameters.$in.path, 'jot/Box');
        assert.strictEqual(sel.parameters.$in.parameters.size, 10);
    });

    await t.test('Box(10).dup() -> $out (Default count)', async () => {
        const script = 'Box(10).dup() -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        const sel = res[0].selector;

        assert.strictEqual(sel.path, 'jot/dup');
        assert.strictEqual(sel.parameters.count, 1);
        assert.strictEqual(sel.parameters.$in.path, 'jot/Box');
    });

    await t.test('Box(10).dup(count=5) -> $out (Named argument)', async () => {
        const script = 'Box(10).dup(count=5) -> $out';
        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
        const sel = res[0].selector;

        assert.strictEqual(sel.path, 'jot/dup');
        assert.strictEqual(sel.parameters.count, 5);
    });
});
