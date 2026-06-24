import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Compiler: a.snap(top(), bottom(), b)', async () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [
                { name: 'width', type: 'jot:number' },
                { name: 'height', type: 'jot:number' },
                { name: 'depth', type: 'jot:number' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('top', {
        path: 'jot/top',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('bottom', {
        path: 'jot/bottom',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('snap', {
        path: 'jot/snap',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'source_anchor', type: 'jot:shape' },
                { name: 'target_anchor', type: 'jot:op<$in:shape, $out:shape>' },
                { name: 'target_shape', type: 'jot:shape' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    const script = 'a = Box(10, 10, 10); b = Box(5, 5, 5); res = a.snap(top(), bottom(), b);';
    const ast = parser.parse(script);
    const params = {};
    const ctx = { symbolTypes: {}, requiredOutputs: new Set() };

    for (const node of ast) {
        await compiler._evaluateRecursive(node, params, null, ctx);
    }

    assert.ok(params.res);
    assert.strictEqual(params.res.path, 'jot/snap');
    
    // Check Snap inputs and arguments
    assert.strictEqual(params.res.parameters.$in.path, 'jot/Box');
    
    // source_anchor should be evaluating top() with subject 'a'
    assert.strictEqual(params.res.parameters.source_anchor.path, 'jot/top');
    assert.strictEqual(params.res.parameters.source_anchor.parameters.$in.path, 'jot/Box');
    
    // target_anchor should be the bottom() recipe
    assert.strictEqual(params.res.parameters.target_anchor.path, 'jot/bottom');
    
    // target_shape should be 'b'
    assert.strictEqual(params.res.parameters.target_shape.path, 'jot/Box');
});
