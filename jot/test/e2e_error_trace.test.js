import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchSystem } from '../../orchestrator.js';
import { TestVFSNode } from '../../fs/test/vfs_test_helpers.js';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { log } from '../../fs/src/log.js';
import { encodeRecord, decodeRecordStream } from '../../fs/src/mesh/forward_connection.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('E2E Failure Trace: Intentional Schema Mismatch', async (t) => {
    let sys;
    let testNode;
    try {
        sys = await launchSystem('test/standard');
        const routerPort = sys.ports.zenoh_router;
        const TEST_NODE_PORT = 9194;

        testNode = new TestVFSNode('test-node', TEST_NODE_PORT, [`http://localhost:${routerPort}`]);
        await testNode.start();

        const boxSchema = { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };
        const rzSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'turns', type: 'jot:any' }], outputs: { $out: 'jot:shape' } };

        const compiler = new JotCompiler(testNode.vfs);
        compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
        compiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

        // INTENTIONAL ERROR: passing a string "wrong" instead of a number
        const mainScript = 'Box(15).rz("wrong") -> $out';
        const terminals = await compiler.evaluate((new JotParser()).parse(mainScript), {}, { outputs: { $out: 'jot:shape' } });
        const finalSelector = terminals[0].selector;

        await assert.rejects(
            async () => {
                await testNode.vfs.readSelector(finalSelector);
            },
            (err) => {
                const errMsg = err.message;
                log('[E2E ERROR TRACE]:');
                errMsg.split('\n').forEach((line, i) => log(`  [${i}] ${line}`));

                assert.ok(errMsg.includes('geo-ops-node') || errMsg.includes('ops'), 'Trace should include the failing node ID');
                assert.ok(errMsg.includes('jot/rotateZ') || errMsg.includes('jot/rz'), 'Trace should include the failing operator');
                assert.ok(errMsg.includes('turns'), 'Trace should include the failing argument name');
                assert.ok(errMsg.includes('type must be number') || errMsg.includes('must be number'), 'Trace should include the original JSON error');
                return true;
            }
        );
        log('--- ERROR TRACE VERIFIED ---');

    } finally {
        if (testNode) await testNode.stop();
        if (sys) await sys.stop();
    }
});
