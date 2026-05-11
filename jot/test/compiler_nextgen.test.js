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
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'tools', type: 'jot:shapes' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('at', {
        path: 'op/at',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'target', type: 'jot:vec3' },
                { name: 'op', type: 'jot:op<$in:shape, $out:shape>' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    await t.test('evaluates template arguments with subject propagation', async () => {
        const ast = parser.parse('Hexagon(30).at([0,0,0], cut(Hexagon(5))) -> $out');
        const schema = { outputs: { "$out": { type: "jot:shape" } } };
        const res_raw = await compiler.evaluate(ast, {}, schema);
        const res = res_raw[0];
        
        const cutOp = res.parameters.op;
        assert.strictEqual(cutOp.path, 'op/cut');
        // The $in for cut() should be inherited from Hexagon(30)
        assert.strictEqual(cutOp.parameters.$in.path, 'op/hexagon');
        assert.strictEqual(cutOp.parameters.$in.parameters.size, 30);
        assert.strictEqual(cutOp.parameters.tools[0].parameters.size, 5);
    });
});
