import assert from 'node:assert';
import { JotCompiler } from '../src/compiler.js';
import { Selector } from '../../fs/src/vfs_core.js';

async function testMetadataAPI() {
    console.log('Testing Metadata API in JOT Compiler...');
    const compiler = new JotCompiler();

    // Mock operators with new 'inputs' schema
    compiler.registerOperator('jot/Box', {
        path: 'jot/Box',
        schema: {
            arguments: [
                { name: 'width', type: 'jot:number', default: 10 },
                { name: 'height', type: 'jot:number', default: 10 }
            ],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('jot/set', {
        path: 'jot/set',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'key', type: 'jot:string' },
                { name: 'value', type: 'jot:any' }
            ],
            outputs: { '$out': { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('jot/get', {
        path: 'jot/get',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'key', type: 'jot:string' }
            ],
            outputs: { '$out': { type: 'jot:any' } }
        }
    });

    // 1. Test Method Chain (Subject Consumption)
    const code = 'Box().set("material", "steel") -> $out';
    const results = await compiler.evaluate(compiler.parser.parse(code), {}, {
        outputs: { '$out': 'jot:shape' }
    });

    const sel = results[0].selector;
    console.log('Generated Selector:', JSON.stringify(sel, null, 2));

    assert.strictEqual(sel.path, 'jot/set');
    assert.strictEqual(sel.parameters.key, 'material');
    assert.strictEqual(sel.parameters.value, 'steel');
    assert.ok(sel.parameters.$in instanceof Selector);
    assert.strictEqual(sel.parameters.$in.path, 'jot/Box');

    // 2. Test Get
    const codeGet = 'Box().get("material") -> $out';
    const resultsGet = await compiler.evaluate(compiler.parser.parse(codeGet), {}, {
        outputs: { '$out': 'jot:any' }
    });

    const selGet = resultsGet[0].selector;
    assert.strictEqual(selGet.path, 'jot/get');
    assert.strictEqual(selGet.parameters.key, 'material');
    assert.ok(selGet.parameters.$in instanceof Selector);

    console.log('✅ Metadata API Compiler Test Passed');
}

// Add parser to compiler for convenience in test
import { JotParser } from '../src/parser.js';
JotCompiler.prototype.parser = new JotParser();

testMetadataAPI().catch(err => {
    console.error('❌ Test Failed:', err);
    process.exit(1);
});
