import './set_env.js';
import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { VFS, DiskStorage, MeshLink, Selector } from '../../fs/src/index.js';
import { launchSystem } from '../../orchestrator.js';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('E2E Integration: Box(15).Red().rz(0.25) -> C++ Cluster Fulfillment', { timeout: 60000 }, async (t) => {
    let sys;
    let jsVfs;
    const STORAGE_JS = path.resolve(__dirname, '../../.vfs_storage_e2e_test');

    const hardTimeout = setTimeout(() => {
        console.error('[Test] Hard timeout reached! Force exiting...');
        process.exit(1);
    }, 25000);
    hardTimeout.unref();

    t.after(async () => {
        console.log('[Test] Cleaning up...');
        clearTimeout(hardTimeout);
        if (jsVfs) await jsVfs.close();
        if (sys) await sys.stop();
        await fs.rm(STORAGE_JS, { recursive: true, force: true }).catch(() => {});
    });

    // 1. Launch the standard test system (router + ops)
    sys = await launchSystem('test/standard');

    // 2. Launch JS Node
    jsVfs = new VFS({
        id: 'js-test-client',
        storage: new DiskStorage(STORAGE_JS),
    });
    const mesh = new MeshLink(jsVfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    await jsVfs.init();
    await mesh.start();

    // 3. Register user/Red on JS VFS
    const boxSchema = { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };
    const colorSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'color', type: 'jot:string' }], outputs: { $out: 'jot:shape' } };
    const redSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };

    jsVfs.registerProvider('user/Red', async (vfs, selector, context) => {
        const requestedPort = selector.output || '$out';
        const innerCompiler = new JotCompiler(vfs);
        innerCompiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
        innerCompiler.registerOperator('color', { path: 'jot/color', schema: colorSchema });

        const parser = new JotParser();
        const ast = parser.parse('$in.color("red") -> $out');
        const terminals = await innerCompiler.evaluate(ast, selector.parameters, redSchema);
        const targetTerminal = terminals.find(t => t.port === requestedPort);
        return await vfs.read(targetTerminal.selector, { ...context, stack: [] });
    }, redSchema);

    // 4. Compile the Main Script
    const rzSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'turns', type: 'jot:numbers' }], outputs: { $out: 'jot:shape' } };
    const compiler = new JotCompiler(jsVfs);
    compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
    compiler.registerOperator('user/Red', { path: 'user/Red', schema: redSchema });
    compiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

    const mainScript = 'Box(15).Red().rz(0.25) -> $out';
    const terminals = await compiler.evaluate((new JotParser()).parse(mainScript), {}, { outputs: { $out: 'jot:shape' } });
    const finalSelector = terminals[0].selector;

    // 5. Read the final selector. This will trigger remote Box, local Red (color), and remote rz natively over Zenoh.
    const resultObj = await jsVfs.read(finalSelector);
    const chunks = [];
    const reader = resultObj.stream.getReader();
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const result = JSON.parse(new TextDecoder().decode(Buffer.concat(chunks)));

    const expected = {
        "geometry": "3946baf8a7f7a6bb24e9eebd1010f19b34437097858d2d071625ce1128750317",
        "tags": {
            "color": "red",
            "type": "surface"
        },
        "tf": "-0 -1 -0 -0 1 0 0 0 0 0 1 0"
    };

    assert.deepStrictEqual(result, expected, 'Returned shape should exactly match the expected expansion result');
});
