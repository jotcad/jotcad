import assert from 'node:assert';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

async function testGreedyPluralAPI() {
    console.log('Testing Greedy Plural API (Union) in JOT Compiler...');
    const compiler = new JotCompiler();

    compiler.registerOperator('jot/Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'width', type: 'jot:number', default: 10 }],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('jot/union', {
        path: 'jot/union',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [{ name: 'tools', type: 'jot:shapes' }],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    // 1. Method Union: Box().union(Box())
    const code = 'Box(10).union(Box(20)) -> $out';
    const results = await compiler.evaluate(compiler.parser.parse(code), {}, {
        outputs: { '$out': 'jot:shape' }
    });

    const sel = results[0].selector;
    console.log('Generated Selector:', JSON.stringify(sel, null, 2));

    assert.strictEqual(sel.path, 'jot/union');
    assert.strictEqual(sel.parameters.$in.parameters.width, 10);
    assert.ok(Array.isArray(sel.parameters.tools));
    assert.strictEqual(sel.parameters.tools.length, 1);
    assert.strictEqual(sel.parameters.tools[0].parameters.width, 20);

    console.log('✅ Greedy Plural API Test Passed');
}

import { JotParser } from '../src/parser.js';
JotCompiler.prototype.parser = new JotParser();

testGreedyPluralAPI().catch(err => {
    console.error('❌ Test Failed:', err);
    process.exit(1);
});
