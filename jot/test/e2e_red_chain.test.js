import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { launchOpsNode } from '../../fs/test/ops_helper.js';
import { TestVFSNode } from '../../fs/test/vfs_test_helpers.js';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { log } from '../../fs/src/log.js';
import { Selector } from '../../fs/src/vfs_core.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');

test('E2E Integration: Box(15).Red().rz(0.25) -> C++ Cluster Fulfillment', async (t) => {
    const OPS_PORT = 9191;
    const TEST_NODE_PORT = 9192;
    const STORAGE_DIR = path.resolve(__dirname, '../../.vfs_storage_e2e_test');

    // 1. Launch C++ Ops Node
    log('[E2E] Launching C++ Ops Node...');
    const ops = await launchOpsNode(OPS_PATH, OPS_PORT, STORAGE_DIR);

    // 2. Launch JS Test Node
    log('[E2E] Launching JS Test Node...');
    const testNode = new TestVFSNode('test-node', TEST_NODE_PORT);
    await testNode.start();

    try {
        // 3. Cross-Register Neighbors
        log('[E2E] Registering Neighbors...');
        await fetch(`http://localhost:${OPS_PORT}/register`, {
            method: 'POST',
            body: JSON.stringify({ id: 'test-node', url: `http://localhost:${TEST_NODE_PORT}` })
        });
        await fetch(`http://localhost:${TEST_NODE_PORT}/register`, {
            method: 'POST',
            body: JSON.stringify({ id: 'geo-ops-node', url: `http://localhost:${OPS_PORT}` })
        });

        // 3.5 Wait for Mesh Handshake & Catalog Exchange
        log('[E2E] Waiting for Mesh Handshake...');
        await new Promise((resolve) => {
            const timeout = setTimeout(resolve, 3000); // 3s Fallback is plenty
            testNode.vfs.events.on('notify', (selector) => {
                if (selector.path === 'sys/schema') {
                    clearTimeout(timeout);
                    resolve();
                }
            });
            testNode.meshLink.addLocalInterest(new Selector('sys/schema'));
        });
        log('[E2E] Mesh Ready.');

        // 4. Setup Operator Schemas (aligned with C++ implementation)
        const boxSchema = { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };
        const colorSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'color', type: 'jot:string' }], outputs: { $out: 'jot:shape' } };
        const rzSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'turns', type: 'jot:numbers' }], outputs: { $out: 'jot:shape' } };
        const redSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };

        // 5. Register User/Red Provider on the JS Test Node
        // ONE SELECTOR PER OUTPUT:
        // A User Operator evaluates its script to a set of 'terminals' (one per wired port).
        // It then fulfills the specific port requested by the VFS caller.
        testNode.registerProvider('user/Red', async (vfs, selector, context) => {
            const requestedPort = selector.output || '$out';
            log(`[TestNode] Fulfilling user/Red port: ${requestedPort}`);
            
            const innerCompiler = new JotCompiler(vfs);
            innerCompiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
            innerCompiler.registerOperator('color', { path: 'jot/color', schema: colorSchema });

            const parser = new JotParser();
            // Script with explicit wiring
            const ast = parser.parse('$in.color("red") -> $out');
            
            // terminals = [{ port: '$out', selector: ... }, { port: 'debug', selector: ... }]
            const terminals = await innerCompiler.evaluate(ast, selector.parameters, redSchema);
            
            const targetTerminal = terminals.find(t => t.port === requestedPort);
            if (!targetTerminal) {
                console.error(`[TestNode] ERROR: Port '${requestedPort}' not found in terminals:`, terminals.map(t => t.port));
                throw new Error(`Port '${requestedPort}' not fulfilled by user/Red script`);
            }

            log(`[TestNode] Expanded ${requestedPort} to Selector:`, JSON.stringify(targetTerminal.selector, null, 2));
            // IMPORTANT: Clear the stack for the sub-read to allow cross-node recursion
            // for the new expanded selector.
            return await vfs.read(targetTerminal.selector, { ...context, stack: [] });
        }, redSchema);

        // 6. Compile the Main Script
        const compiler = new JotCompiler();
        compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
        compiler.registerOperator('user/Red', { path: 'user/Red', schema: redSchema });
        compiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

        const mainScript = 'Box(15).Red().rz(0.25) -> $out';
        const terminals = await compiler.evaluate((new JotParser()).parse(mainScript), {}, { outputs: { $out: 'jot:shape' } });
        const finalSelector = terminals[0].selector;

        log('[E2E] Final Selector:', JSON.stringify(finalSelector, null, 2));

        // 7. Request Execution from the C++ Ops Node
        // The Ops Node will call back to the Test Node for user/Red.
        // The Test Node will resolve the expansion and return the result.
        const response = await fetch(`http://localhost:${OPS_PORT}/read`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ selector: finalSelector.toJSON() })
        });

        if (!response.ok) {
            const errBody = await response.text();
            console.error('[E2E FAILURE TRACE]:');
            errBody.split('\n').forEach((line, i) => {
                if (line.trim()) {
                    try {
                        const parsed = JSON.parse(line);
                        console.error(`  [${i}]`, parsed);
                    } catch (e) {
                        console.error(`  [${i}] ${line}`);
                    }
                }
            });
            throw new Error(`Ops Node failed: See trace above`);
        }

        const result = await response.json();
        log('[E2E] Execution Result:', JSON.stringify(result, null, 2));

        // 8. Assertions: Deep equal vs Expected Shape
        // Note: The geometry hash is for a 15x15x15 Box.
        // The matrix reflects a 0.25 (Tau) rotation around Z.
        const expected = {
            "geometry": "3946baf8a7f7a6bb24e9eebd1010f19b34437097858d2d071625ce1128750317",
            "tags": {
                "color": "red",
                "type": "surface"
            },
            "tf": "-0 -1 -0 -0 1 0 0 0 0 0 1 0"
        };

        assert.deepStrictEqual(result, expected, 'Returned shape should exactly match the expected expansion result');
        
        log('--- E2E SUCCESS ---');
        log('Final Shape JSON verified successfully.');

    } finally {
        log('[E2E] Cleaning up...');
        await testNode.stop();
        await ops.stop();
    }
});
