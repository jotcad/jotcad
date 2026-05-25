import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchOpsNode } from '../../fs/test/ops_helper.js';
import { TestVFSNode } from '../../fs/test/vfs_test_helpers.js';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { log } from '../../fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');

test('E2E Failure Trace: Intentional Schema Mismatch', async (t) => {
    const OPS_PORT = 9193;
    const TEST_NODE_PORT = 9194;
    const STORAGE_DIR = path.resolve(__dirname, '../../.vfs_storage_e2e_error');

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
        // INTENTIONAL ERROR: 'number' (singular) instead of 'numbers'
        const rzSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'turns', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };

        const compiler = new JotCompiler();
        compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
        compiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

        const mainScript = 'Box(15).rz(0.25) -> $out';
        const terminals = await compiler.evaluate((new JotParser()).parse(mainScript), {}, { outputs: { $out: 'jot:shape' } });
        const finalSelector = terminals[0].selector;

        const response = await fetch(`http://localhost:${OPS_PORT}/read`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ selector: finalSelector.toJSON() })
        });

        assert.strictEqual(response.status, 500, 'Should fail with 500');
        const errBody = await response.text();
        
        log('[E2E ERROR TRACE]:');
        errBody.split('\n').forEach((line, i) => log(`  [${i}] ${line}`));

        assert.ok(errBody.includes('geo-ops-node'), 'Trace should include the failing node ID');
        assert.ok(errBody.includes('jot/rotateZ'), 'Trace should include the failing operator');
        assert.ok(errBody.includes('turns'), 'Trace should include the failing argument name');
        assert.ok(errBody.includes('type must be array'), 'Trace should include the original JSON error');

        log('--- ERROR TRACE VERIFIED ---');

    } finally {
        await testNode.stop();
        await ops.stop();
    }
});
