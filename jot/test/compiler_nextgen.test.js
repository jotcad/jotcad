import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCAD Next-Gen: Template Modes', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    compiler.registerOperator('Hexagon', {
        path: 'op/hexagon',
        schema: {
            arguments: [{ name: 'size', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('cut', {
        path: 'op/cut',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'tools', type: 'jot:shapes' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('at', {
        path: 'op/at',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'target', type: 'jot:vec3' },
                { name: 'op', type: 'jot:op<$in:jot:shape, $out:jot:shape>' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    await t.test('evaluates template arguments with subject propagation', async () => {
        const ast = parser.parse('Hexagon(30).at([0,0,0], cut(Hexagon(5))) -> $out');
        const schema = { outputs: { "$out": { type: "jot:shape" } } };
        const res_raw = await compiler.evaluate(ast, {}, schema);
        const res = res_raw[0].selector;
        
        assert.strictEqual(res.path, 'op/at');
        // The top-level $in is bound to Hexagon(30)
        assert.strictEqual(res.parameters.$in.path, 'op/hexagon');
        assert.strictEqual(res.parameters.$in.parameters.size, 30);

        const cutOp = res.parameters.op;
        assert.strictEqual(cutOp.path, 'op/cut');
        assert.strictEqual(cutOp.parameters.tools[0].parameters.size, 5);
        // Recipe $in remains unbound at compile-time for kernel dynamic binding
        assert.strictEqual(cutOp.parameters.$in, undefined);
    });
});
