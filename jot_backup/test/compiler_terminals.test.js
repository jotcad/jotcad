import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCompiler Terminal Discovery', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    // 1. Register Box (Standard Shape Op)
    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'size', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    // 2. Register PDF (Tee Op with multiple outputs)
    compiler.registerOperator('pdf', {
        path: 'jot/pdf',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape' },
                { name: 'path', type: 'jot:string' }
            ],
            outputs: { 
                "$out": { type: "file" }
            }
        }
    });

    await t.test('Box(10).pdf("x") produces a file terminal on $out', async () => {
        const ast = parser.parse('Box(10).pdf("x")');
        const terminals = await compiler.evaluate(ast);
        
        // We expect exactly ONE terminal:
        // 1. jot/pdf:$out (The PDF artifact)
        
        assert.strictEqual(terminals.length, 1, 'Should have exactly 1 terminal output');
        
        const fileTerm = terminals.find(t => t.path === 'jot/pdf' && (t.output === '$out' || !t.output));
        assert.ok(fileTerm, 'Should have a jot/pdf:$out terminal');
        
        // Ensure Box:$out is CONSUMED (not a terminal)
        const boxTerm = terminals.find(t => t.path === 'jot/Box');
        assert.ok(!boxTerm, 'Box should be consumed and NOT a terminal');
    });

    await t.test('Multiple shapes Box(10); Box(20) produces two terminals', async () => {
        const ast = parser.parse('Box(10); Box(20)');
        const terminals = await compiler.evaluate(ast);
        
        assert.strictEqual(terminals.length, 2, 'Should have 2 terminal shapes');
        assert.ok(terminals.some(t => t.parameters.size === 10));
        assert.ok(terminals.some(t => t.parameters.size === 20));
    });

    await t.test('Nested shapes Box(Box(10)) consumption check', async () => {
        // Redefine Box to accept a shape as well
        compiler.registerOperator('BoxWrap', {
            path: 'jot/BoxWrap',
            schema: {
                arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$in' }],
                outputs: { "$out": { type: "jot:shape" } }
            }
        });

        const ast = parser.parse('BoxWrap(Box(10))');
        const terminals = await compiler.evaluate(ast);
        
        assert.strictEqual(terminals.length, 1, 'Only the outer BoxWrap should be a terminal');
        assert.strictEqual(terminals[0].path, 'jot/BoxWrap');
    });
});
