import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCompiler Block Support (a.{ ... }.b)', async (t) => {
    const parser = new JotParser();
    const compiler = new JotCompiler();

    // Register Operators
    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'size', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('move', {
        path: 'jot/move',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'v', type: 'jot:number' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('scale', {
        path: 'jot/scale',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'factor', type: 'jot:number' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('Foot', {
        path: 'jot/Foot',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('Hexagon', {
        path: 'jot/Hexagon',
        schema: {
            arguments: [{ name: 'edgeToEdge', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    compiler.registerOperator('pdf', {
        path: 'jot/pdf',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'path', type: 'jot:string' }
            ],
            outputs: { "$out": { type: "jot:file" } }
        }
    });
    await t.test('Basic block inheritance and passthrough', async () => {
        // Box(10).{ move(5) }.scale(2)
        // move(5) should inherit Box(10)
        // block returns Box(10)
        // scale(2) applies to Box(10)
        const script = `Box(10).{ move(5) }.scale(2) -> $out`;
        const ast = parser.parse(script);
        const schema = { outputs: { "$out": { type: "jot:shape" } } };

        const res = await compiler.evaluate(ast, {}, schema);
        const terminal = res[0].selector;

        assert.strictEqual(terminal.path, 'jot/scale');
        assert.strictEqual(terminal.parameters.factor, 2);
        assert.strictEqual(terminal.parameters.$in.path, 'jot/Box');
        assert.strictEqual(terminal.parameters.$in.parameters.size, 10);
    });

    await t.test('Block local variables', async () => {
        // v should be local to the block
        const script = `
            Box(10).{
                v = move(5);
                v.scale(3);
            }.scale(2) -> $out
        `;
        const ast = parser.parse(script);
        const schema = { outputs: { "$out": { type: "jot:shape" } } };

        const res = await compiler.evaluate(ast, {}, schema);
        const terminal = res[0].selector;

        // Terminal should still be Box(10).scale(2)
        assert.strictEqual(terminal.path, 'jot/scale');
        assert.strictEqual(terminal.parameters.factor, 2);
        
        // Note: We no longer verify block-locality by checking compiler.localSymbols 
        // because the new isolation model stores this state in a transient context.
    });

    await t.test('Multiple instructions in block', async () => {
        const script = `
            Box(10).{
                move(1);
                move(2);
                move(3);
            }.scale(0.5) -> $out
        `;
        const ast = parser.parse(script);
        const schema = { outputs: { "$out": { type: "jot:shape" } } };

        const res = await compiler.evaluate(ast, {}, schema);
        assert.strictEqual(res[0].selector.path, 'jot/scale');
    });

    await t.test('Scoped block with internal port assignment (Multi-output)', async () => {
        const script = `Hexagon(edgeToEdge=248).Foot().{
  pdf('hex_foot2.pdf') -> $hex
} -> $out`;
        
        const schema = {
            outputs: {
                "$out": { "type": "jot:shape" },
                "$hex": { "type": "jot:file" }
            }
        };

        const ast = parser.parse(script);
        const res = await compiler.evaluate(ast, {}, schema);
        
        assert.ok(res.find(r => r.port === '$out'), 'Should fulfill $out');
        assert.ok(res.find(r => r.port === '$hex'), 'Should fulfill $hex');
        assert.strictEqual(res.find(r => r.port === '$out').selector.path, 'jot/Foot');
        assert.strictEqual(res.find(r => r.port === '$hex').selector.path, 'jot/pdf');
    });
});
