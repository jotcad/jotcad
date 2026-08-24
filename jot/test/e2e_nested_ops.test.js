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

test('E2E Nested Expansion: Op A calls Op B', async (t) => {
    let sys;
    let testNode;
    try {
        sys = await launchSystem('test/standard');
        const routerPort = sys.ports.zenoh_router;
        const TEST_NODE_PORT = 0;

        testNode = new TestVFSNode('test-node', TEST_NODE_PORT, [`http://localhost:${routerPort}`]);
        await testNode.start();

        // Sync live catalog for real C++ operators (Box, color, etc.)
        const catalog = await testNode.meshLink.fetchCatalog({ timeout: 5000 });

        const opBSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };
        const opASchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };

        // Register Op B: Wraps color("blue")
        testNode.registerProvider('user/OpB', async (vfs, selector, context) => {
            const innerCompiler = new JotCompiler(vfs);
            for (const [path, schema] of Object.entries(catalog)) {
                innerCompiler.registerOperator(path, { path, schema });
            }
            const terminals = await innerCompiler.evaluate((new JotParser()).parse('$in.color("blue") -> $out'), selector.parameters, opBSchema);
            return await vfs.readSelector(terminals[0].selector, { ...context, stack: [] });
        }, opBSchema);

        // Register Op A: Wraps Op B
        testNode.registerProvider('user/OpA', async (vfs, selector, context) => {
            const innerCompiler = new JotCompiler(vfs);
            for (const [path, schema] of Object.entries(catalog)) {
                innerCompiler.registerOperator(path, { path, schema });
            }
            innerCompiler.registerOperator('user/OpB', { path: 'user/OpB', schema: opBSchema });
            const terminals = await innerCompiler.evaluate((new JotParser()).parse('$in.OpB() -> $out'), selector.parameters, opASchema);
            return await vfs.readSelector(terminals[0].selector, { ...context, stack: [] });
        }, opASchema);

        // Execute Box(10, 10, 10).OpA()
        const compiler = new JotCompiler(testNode.vfs);
        for (const [path, schema] of Object.entries(catalog)) {
            compiler.registerOperator(path, { path, schema });
        }
        compiler.registerOperator('user/OpA', { path: 'user/OpA', schema: opASchema });
        const terminals = await compiler.evaluate((new JotParser()).parse('Box(10, 10, 10).OpA() -> $out'), {}, { outputs: { $out: 'jot:shape' } });

        const resultObj = await testNode.vfs.readSelector(terminals[0].selector);

        const chunks = [];
        const reader = resultObj.stream.getReader();
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            chunks.push(value);
        }
        const result = JSON.parse(new TextDecoder().decode(Buffer.concat(chunks)));
        assert.strictEqual(result.tags?.color, 'blue', 'Nested expansion should result in blue color');
        log('--- NESTED EXPANSION SUCCESS ---');

    } finally {
        if (testNode) await testNode.stop();
        if (sys) await sys.stop();
    }
});
