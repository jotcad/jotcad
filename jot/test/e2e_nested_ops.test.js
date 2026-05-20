import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchOpsNode } from '../../fs/test/ops_helper.js';
import { TestVFSNode } from '../../fs/test/vfs_test_helpers.js';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');

test('E2E Nested Expansion: Op A calls Op B', async (t) => {
    const OPS_PORT = 9195;
    const TEST_NODE_PORT = 9196;
    const STORAGE_DIR = path.resolve(__dirname, '../../.vfs_storage_e2e_nested');

    const ops = await launchOpsNode(OPS_PATH, OPS_PORT, STORAGE_DIR);
    const testNode = new TestVFSNode('test-node', TEST_NODE_PORT);
    await testNode.start();

    try {
        await fetch(`http://localhost:${OPS_PORT}/register`, {
            method: 'POST',
            body: JSON.stringify({ id: 'test-node', url: `http://localhost:${TEST_NODE_PORT}` })
        });
        await fetch(`http://localhost:${TEST_NODE_PORT}/register`, {
            method: 'POST',
            body: JSON.stringify({ id: 'geo-ops-node', url: `http://localhost:${OPS_PORT}` })
        });

        const boxSchema = { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };
        const colorSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'color', type: 'jot:string' }], outputs: { $out: 'jot:shape' } };
        const opBSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };
        const opASchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };

        // Register Op B: Wraps color("blue")
        testNode.registerProvider('user/OpB', async (vfs, selector, context) => {
            const innerCompiler = new JotCompiler(vfs);
            innerCompiler.registerOperator('color', { path: 'jot/color', schema: colorSchema });
            const terminals = await innerCompiler.evaluate((new JotParser()).parse('$in.color("blue") -> $out'), selector.parameters, opBSchema);
            return await vfs.read(terminals[0].selector, { ...context, stack: [] });
        }, opBSchema);

        // Register Op A: Wraps Op B
        testNode.registerProvider('user/OpA', async (vfs, selector, context) => {
            const innerCompiler = new JotCompiler(vfs);
            innerCompiler.registerOperator('user/OpB', { path: 'user/OpB', schema: opBSchema });
            const terminals = await innerCompiler.evaluate((new JotParser()).parse('$in.OpB() -> $out'), selector.parameters, opASchema);
            return await vfs.read(terminals[0].selector, { ...context, stack: [] });
        }, opASchema);

        // Execute Box().OpA()
        const compiler = new JotCompiler();
        compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
        compiler.registerOperator('user/OpA', { path: 'user/OpA', schema: opASchema });
        const terminals = await compiler.evaluate((new JotParser()).parse('Box(10).OpA() -> $out'), {}, { outputs: { $out: 'jot:shape' } });

        const response = await fetch(`http://localhost:${OPS_PORT}/read`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ selector: terminals[0].selector.toJSON() })
        });

        if (!response.ok) {
            const err = await response.text();
            throw new Error(`Execution failed: ${err}`);
        }

        const result = await response.json();
        assert.strictEqual(result.tags?.color, 'blue', 'Nested expansion should result in blue color');
        console.log('--- NESTED EXPANSION SUCCESS ---');

    } finally {
        await testNode.stop();
        await ops.stop();
    }
});
