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

    // FAIL: Fails with TypeError because HOLE constant was removed and unprovided
    // arguments are now undefined.
    await t.test('evaluates template arguments with subject HOLE', async () => {
        const ast = parser.parse('Hexagon(30).at([0,0,0], cut(Hexagon(5)))');
        const res = await compiler.evaluate(ast);
        
        const cutOp = res.parameters.op;
        assert.strictEqual(cutOp.path, 'op/cut');
        // The $in for cut() should be undefined (a hole)
        assert.strictEqual(cutOp.parameters.$in, undefined);
        assert.strictEqual(cutOp.parameters.tools[0].parameters.size, 5);
    });
});
