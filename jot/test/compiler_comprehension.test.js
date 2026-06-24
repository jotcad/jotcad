import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Map Comprehension: Basic Numeric Projection', async () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();
    const ctx = { symbolTypes: {}, requiredOutputs: new Set() };

    // [[1, 2, 3] do x => x]
    const script = '[[1, 2, 3] do x => x]';

    const ast = parser.parse(script);
    const results = await compiler._evaluateRecursive(ast, {}, null, ctx);

    assert.deepStrictEqual(results, [1, 2, 3]);
});

test('Map Comprehension: Range Projection (Inclusive)', async () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();
    const ctx = { symbolTypes: {}, requiredOutputs: new Set() };

    // [0..2 inc do $ => [0, $, $]]
    const script = '[0..2 inc do $ => [0, $, $]]';

    const ast = parser.parse(script);
    const results = await compiler._evaluateRecursive(ast, {}, null, ctx);

    assert.deepStrictEqual(results, [[0, 0, 0], [0, 1, 1], [0, 2, 2]]);
});

test('Map Comprehension: List Projection', async () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();
    const ctx = { symbolTypes: {}, requiredOutputs: new Set() };

    // [1, 2, 3 do i => [i, i]] -> [[1, 1], [2, 2], [3, 3]]
    const script = '[1, 2, 3 do i => [i, i]]';
    const ast = parser.parse(script);
    const results = await compiler._evaluateRecursive(ast, {}, null, ctx);

    assert.deepStrictEqual(results, [[1, 1], [2, 2], [3, 3]]);
});

test('Map Comprehension: Lexical Scoping', async () => {
    const parser = new JotParser();
    const compiler = new JotCompiler();
    const ctx = { symbolTypes: {}, requiredOutputs: new Set() };

    // x = 10; res = [1, 2 do x => x] -> [1, 2]
    const script = 'x = 10; res = [1, 2 do x => x]';
    const ast = parser.parse(script);
    const params = {};
    
    // Evaluate sequentially
    for (const node of ast) {
        await compiler._evaluateRecursive(node, params, null, ctx);
    }

    assert.strictEqual(params.x, 10);
    assert.deepStrictEqual(params.res, [1, 2]);
});
